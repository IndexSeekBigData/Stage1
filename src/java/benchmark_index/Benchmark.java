
package benchmark_index;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.HashMap;
import java.util.HashSet;
import java.util.stream.Stream;

public class Benchmark {

    private static final Path ROOT = Path.of("").toAbsolutePath();
    private static final Path SAMPLE_DATA = ROOT.resolve("sample_data");
    private static final Path INDEX_PATH = ROOT.resolve("index_benchmark/java");
    private static final Path RESULTS_PATH = ROOT.resolve("benchmark");

    private static final int REPETITIONS = 3;
    private static final int QUERY_REPETITIONS = 100;

    private static final List<String> QUERIES = List.of(
            "the", "and", "love", "man", "woman",
            "time", "house", "book", "zzzznotfound"
    );

    private record Stats(double mean, double std) {}

    private record IncrementalResult(
            String structure,
            Stats incremental,
            Stats rebuild,
            int affectedTerms
    ) {}

    private static Map<String, InvertedIndex> structures() {
        Map<String, InvertedIndex> result = new LinkedHashMap<>();

        result.put(
                "monolithic",
                new MonolithicJson(INDEX_PATH.resolve("monolithic"))
        );

        result.put(
                "term_files",
                new TermFiles(INDEX_PATH.resolve("term_files"))
        );

        result.put(
                "hierarchical",
                new Hierarchical(INDEX_PATH.resolve("hierarchical"))
        );

        return result;
    }

    private static List<Book> loadBooks() throws IOException {
        List<Book> books = new ArrayList<>();

        if (!Files.isDirectory(SAMPLE_DATA)) {
            throw new IOException(
                    "No existe sample_data: " + SAMPLE_DATA
            );
        }

        try (Stream<Path> stream = Files.list(SAMPLE_DATA)) {
            List<Path> paths = stream
                    .filter(path -> path.getFileName()
                            .toString().endsWith(".body.txt"))
                    .sorted(Comparator.comparing(
                            path -> path.getFileName().toString()
                    ))
                    .toList();

            for (Path path : paths) {
                String filename = path.getFileName().toString();
                String id = filename.split("\\.")[0];

                String body = Files.readString(
                        path,
                        StandardCharsets.UTF_8
                );

                books.add(new Book(id, body));
            }
        }

        return books;
    }

    private static void cleanIndexes() throws IOException {
        if (!Files.exists(INDEX_PATH)) {
            return;
        }

        try (Stream<Path> stream = Files.walk(INDEX_PATH)) {
            for (Path path : stream
                    .sorted(Comparator.reverseOrder())
                    .toList()) {
                Files.delete(path);
            }
        }
    }

    private static void buildAllIndexes(
            List<Book> books
    ) throws IOException {
        cleanIndexes();

        for (InvertedIndex index : structures().values()) {
            index.buildIndex(books);
        }
    }

    private static Stats summarize(List<Double> times) {
        double mean = times.stream()
                .mapToDouble(Double::doubleValue)
                .average()
                .orElse(0.0);

        double sum = 0.0;

        for (double value : times) {
            sum += Math.pow(value - mean, 2);
        }

        double std = times.size() > 1
                ? Math.sqrt(sum / (times.size() - 1))
                : 0.0;

        return new Stats(mean, std);
    }

    private static void benchmarkIndexing(
            List<Book> books
    ) throws IOException {
        System.out.println("\n========== FULL INDEX BUILD ==========");

        for (Map.Entry<String, InvertedIndex> entry
                : structures().entrySet()) {

            List<Double> times = new ArrayList<>();

            for (int repetition = 0;
                 repetition < REPETITIONS;
                 repetition++) {

                cleanIndexes();

                long start = System.nanoTime();
                entry.getValue().buildIndex(books);
                double elapsed =
                        (System.nanoTime() - start) / 1_000_000_000.0;

                times.add(elapsed);
            }

            Stats stats = summarize(times);

            System.out.printf(
                    Locale.US,
                    "%-12s | %.6f s | std: %.6f s | %.2f libros/s%n",
                    entry.getKey(),
                    stats.mean(),
                    stats.std(),
                    books.size() / stats.mean()
            );
        }
    }

    private static void benchmarkQueries()
            throws IOException {
        System.out.println("\n========== QUERY PERFORMANCE ==========");
        System.out.println("Consultas: " + QUERIES);

        for (Map.Entry<String, InvertedIndex> entry
                : structures().entrySet()) {

            List<Double> times = new ArrayList<>();

            for (int repetition = 0;
                 repetition < REPETITIONS;
                 repetition++) {

                long start = System.nanoTime();

                for (int i = 0; i < QUERY_REPETITIONS; i++) {
                    for (String term : QUERIES) {
                        entry.getValue().searchTerm(term);
                    }
                }

                double elapsedMs =
                        (System.nanoTime() - start) / 1_000_000.0;

                int total = QUERY_REPETITIONS * QUERIES.size();
                times.add(elapsedMs / total);
            }

            Stats stats = summarize(times);

            System.out.printf(
                    Locale.US,
                    "%-12s | %.6f ms/consulta | std: %.6f ms%n",
                    entry.getKey(),
                    stats.mean(),
                    stats.std()
            );
        }
    }

    private static Map<String, Set<String>> expectedPostings(
            List<Book> books
    ) {
        Map<String, Set<String>> expected = new HashMap<>();

        for (Book book : books) {
            for (String term : Tokenizer.tokenize(book.body())) {
                expected
                        .computeIfAbsent(term, key -> new HashSet<>())
                        .add(book.id());
            }
        }

        return expected;
    }

    private static void verifyTerm(
            String name,
            InvertedIndex index,
            String term,
            Map<String, Set<String>> expected
    ) throws IOException {

        List<String> returned = index.searchTerm(term);
        Set<String> returnedSet = new HashSet<>(returned);
        Set<String> expectedSet =
                expected.getOrDefault(term, Set.of());

        if (returned.size() != returnedSet.size()) {
            throw new IllegalStateException(
                    name + ": IDs duplicados para " + term
            );
        }

        if (!returnedSet.equals(expectedSet)) {
            throw new IllegalStateException(
                    name + ": resultados incorrectos para " + term
                            + ". Esperados: " + expectedSet
                            + ". Obtenidos: " + returnedSet
            );
        }
    }

    private static void verifyQueries(
            List<Book> books,
            String stage
    ) throws IOException {

        Map<String, Set<String>> expected =
                expectedPostings(books);

        System.out.println(
                "\n========== QUERY CORRECTNESS: "
                        + stage + " =========="
        );

        for (String term : QUERIES) {
            for (Map.Entry<String, InvertedIndex> entry
                    : structures().entrySet()) {

                verifyTerm(
                        entry.getKey(),
                        entry.getValue(),
                        term,
                        expected
                );
            }

            System.out.printf(
                    "%-15s | %d documentos | OK%n",
                    term,
                    expected.getOrDefault(term, Set.of()).size()
            );
        }
    }

    private static void benchmarkIncremental(
            List<Book> books
    ) throws IOException {

        if (books.size() < 10) {
            System.out.println(
                    "\nNo hay suficientes libros para incremental."
            );
            return;
        }

        List<Book> baseBooks =
                new ArrayList<>(books.subList(0, books.size() - 10));

        List<Book> newBooks =
                new ArrayList<>(books.subList(books.size() - 10, books.size()));

        Set<String> expectedTerms = new HashSet<>();

        for (Book book : newBooks) {
            expectedTerms.addAll(Tokenizer.tokenize(book.body()));
        }

        Map<String, Set<String>> expected =
                expectedPostings(books);

        System.out.println(
                "\n========== INCREMENTAL VS FULL REBUILD =========="
        );
        System.out.println("Libros iniciales: " + baseBooks.size());
        System.out.println("Libros nuevos: " + newBooks.size());
        System.out.println(
                "IDs nuevos: "
                        + newBooks.stream().map(Book::id).toList()
        );
        System.out.println(
                "Términos afectados esperados: " + expectedTerms.size()
        );

        List<IncrementalResult> results = new ArrayList<>();

        for (Map.Entry<String, InvertedIndex> entry
                : structures().entrySet()) {

            String name = entry.getKey();
            InvertedIndex index = entry.getValue();

            List<Double> incrementalTimes = new ArrayList<>();
            List<Double> rebuildTimes = new ArrayList<>();

            for (int repetition = 0;
                 repetition < REPETITIONS;
                 repetition++) {

                cleanIndexes();
                index.buildIndex(baseBooks);

                long start = System.nanoTime();
                Set<String> affected = index.updateIndex(newBooks);

                double incrementalElapsed =
                        (System.nanoTime() - start) / 1_000_000_000.0;

                if (!affected.equals(expectedTerms)) {
                    throw new IllegalStateException(
                            name + ": términos afectados incorrectos"
                    );
                }

                incrementalTimes.add(incrementalElapsed);

                for (String term : QUERIES) {
                    verifyTerm(name, index, term, expected);
                }

                cleanIndexes();

                start = System.nanoTime();
                index.buildIndex(books);

                double rebuildElapsed =
                        (System.nanoTime() - start) / 1_000_000_000.0;

                rebuildTimes.add(rebuildElapsed);
            }

            Stats incrementalStats = summarize(incrementalTimes);
            Stats rebuildStats = summarize(rebuildTimes);

            results.add(new IncrementalResult(
                    name,
                    incrementalStats,
                    rebuildStats,
                    expectedTerms.size()
            ));

            System.out.println("\n" + name);

            System.out.printf(
                    Locale.US,
                    "Actualización incremental: %.6f s (std: %.6f)%n",
                    incrementalStats.mean(),
                    incrementalStats.std()
            );

            System.out.printf(
                    Locale.US,
                    "Reconstrucción completa: %.6f s (std: %.6f)%n",
                    rebuildStats.mean(),
                    rebuildStats.std()
            );

            System.out.println(
                    "Términos afectados: " + expectedTerms.size()
            );
        }

        System.out.println(
                "\nVerificación incremental: OK en las tres estructuras"
        );

        Files.createDirectories(RESULTS_PATH);

        Path csv = RESULTS_PATH.resolve(
                "java_index_incremental.csv"
        );

        List<String> lines = new ArrayList<>();

        lines.add(
                "structure,incremental_mean_s,incremental_std_s,"
                        + "rebuild_mean_s,rebuild_std_s,affected_terms"
        );

        for (IncrementalResult result : results) {
            lines.add(String.format(
                    Locale.US,
                    "%s,%.6f,%.6f,%.6f,%.6f,%d",
                    result.structure(),
                    result.incremental().mean(),
                    result.incremental().std(),
                    result.rebuild().mean(),
                    result.rebuild().std(),
                    result.affectedTerms()
            ));
        }

        Files.write(csv, lines, StandardCharsets.UTF_8);

        System.out.println("\nResultados guardados en: " + csv);
    }

    private static long directorySize(Path path)
            throws IOException {

        if (!Files.exists(path)) {
            return 0;
        }

        try (Stream<Path> stream = Files.walk(path)) {
            return stream
                    .filter(Files::isRegularFile)
                    .mapToLong(file -> {
                        try {
                            return Files.size(file);
                        } catch (IOException e) {
                            throw new java.io.UncheckedIOException(e);
                        }
                    })
                    .sum();
        }
    }

    private static long countFiles(Path path)
            throws IOException {

        if (!Files.exists(path)) {
            return 0;
        }

        try (Stream<Path> stream = Files.walk(path)) {
            return stream.filter(Files::isRegularFile).count();
        }
    }

    private static long countDirectories(Path path)
            throws IOException {

        if (!Files.exists(path)) {
            return 0;
        }

        try (Stream<Path> stream = Files.walk(path)) {
            return stream
                    .filter(Files::isDirectory)
                    .filter(folder -> !folder.equals(path))
                    .count();
        }
    }

    private static void benchmarkStorage()
            throws IOException {

        System.out.println("\n========== STORAGE ==========");

        for (String name : structures().keySet()) {
            Path path = INDEX_PATH.resolve(name);

            double sizeMb =
                    directorySize(path) / (1024.0 * 1024.0);

            System.out.printf(
                    Locale.US,
                    "%-12s | %.2f MB | %d archivos | %d directorios%n",
                    name,
                    sizeMb,
                    countFiles(path),
                    countDirectories(path)
            );
        }
    }

    public static void main(String[] args)
            throws IOException {

        List<Book> books = loadBooks();

        System.out.println("========================================");
        System.out.println("    INVERTED INDEX BENCHMARK - JAVA");
        System.out.println("========================================");
        System.out.println("Libros cargados: " + books.size());
        System.out.println("Repeticiones: " + REPETITIONS);
        System.out.println("Sample data: " + SAMPLE_DATA);
        System.out.println("Benchmark path: " + INDEX_PATH);

        if (books.isEmpty()) {
            System.out.println(
                    "No se encontraron libros en sample_data."
            );
            return;
        }

        benchmarkIndexing(books);

        buildAllIndexes(books);

        benchmarkQueries();
        verifyQueries(books, "FULL BUILD");

        benchmarkIncremental(books);

        buildAllIndexes(books);

        verifyQueries(books, "AFTER REBUILD");
        benchmarkStorage();

        System.out.println("\n========================================");
        System.out.println("          BENCHMARK COMPLETADO");
        System.out.println("========================================");
    }
}
