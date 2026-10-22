
package benchmark_datalake;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.stream.Stream;

public class Benchmark {

    private static final Path PROJECT_ROOT = Paths.get("").toAbsolutePath();
    private static final Path SAMPLE_DATA_PATH = PROJECT_ROOT.resolve("sample_data");
    private static final Path BENCHMARK_PATH = PROJECT_ROOT.resolve("datalake_benchmark/java");
    private static final Path RESULTS_PATH = PROJECT_ROOT.resolve("benchmark");

    private static final int REPETITIONS = 5;
    private static final int LOOKUP_REPETITIONS = 100;
    private static final int RANGE_SIZE = 25;

    private static final String DATE = "20261004";
    private static final String HOUR = "22";

    private static final List<String> STRUCTURES = List.of(
            "time_based",
            "book_based",
            "range_based"
    );

    private record Book(String id, String header, String body) {}

    private record Stats(double mean, double std) {}

    private record Storage(double megabytes, long files, long directories) {}

    private static List<Book> loadBooks() throws IOException {
        List<Book> books = new ArrayList<>();

        try (Stream<Path> stream = Files.list(SAMPLE_DATA_PATH)) {
            List<Path> paths = stream
                    .filter(path -> path.getFileName().toString().endsWith(".body.txt"))
                    .sorted(Comparator.comparingInt(
                            path -> Integer.parseInt(
                                    path.getFileName().toString().split("\\.")[0]
                            )
                    ))
                    .toList();

            for (Path bodyPath : paths) {
                String id = bodyPath.getFileName().toString().split("\\.")[0];
                Path headerPath = SAMPLE_DATA_PATH.resolve(id + ".header.txt");

                if (!Files.isRegularFile(headerPath)) {
                    continue;
                }

                books.add(new Book(
                        id,
                        Files.readString(headerPath, StandardCharsets.UTF_8),
                        Files.readString(bodyPath, StandardCharsets.UTF_8)
                ));
            }
        }

        return books;
    }

    private static void clean(Path path) throws IOException {
        if (!Files.exists(path)) {
            return;
        }

        try (Stream<Path> stream = Files.walk(path)) {
            for (Path entry : stream.sorted(Comparator.reverseOrder()).toList()) {
                Files.delete(entry);
            }
        }
    }

    private static void cleanStructure(String structure) throws IOException {
        clean(BENCHMARK_PATH.resolve(structure));
    }

    private static void cleanAll() throws IOException {
        for (String structure : STRUCTURES) {
            cleanStructure(structure);
        }
    }

    private static Datalake create(String structure) {
        Path path = BENCHMARK_PATH.resolve(structure);

        return switch (structure) {
            case "time_based" -> new TimeBased(path, DATE, HOUR);
            case "book_based" -> new BookBased(path);
            case "range_based" -> new RangeBased(path);
            default -> throw new IllegalArgumentException(structure);
        };
    }

    private static void saveBook(String structure, Book book) throws IOException {
        create(structure).saveBook(
                book.id(),
                book.header(),
                book.body()
        );
    }

    private static void saveBooks(String structure, List<Book> books) throws IOException {
        Datalake lake = create(structure);

        for (Book book : books) {
            lake.saveBook(
                    book.id(),
                    book.header(),
                    book.body()
            );
        }
    }

    private static void saveAllBooks(List<Book> books) throws IOException {
        for (String structure : STRUCTURES) {
            saveBooks(structure, books);
        }
    }

    private static Path lookup(String structure, String bookId) throws IOException {
        Path base = BENCHMARK_PATH.resolve(structure);

        return switch (structure) {
            case "time_based" -> {
                Path result = null;

                try (Stream<Path> stream = Files.walk(base)) {
                    result = stream
                            .filter(path -> Files.isRegularFile(path)
                                    && path.getFileName().toString().equals(bookId + ".body.txt"))
                            .findFirst()
                            .orElse(null);
                }

                yield result;
            }

            case "book_based" -> {
                Path path = base
                        .resolve(bookId)
                        .resolve(bookId + ".body.txt");

                yield Files.isRegularFile(path) ? path : null;
            }

            case "range_based" -> {
                int number = Integer.parseInt(bookId);
                int start = (number / RANGE_SIZE) * RANGE_SIZE;
                int end = start + RANGE_SIZE - 1;

                Path path = base
                        .resolve(start + "-" + end)
                        .resolve(bookId + ".body.txt");

                yield Files.isRegularFile(path) ? path : null;
            }

            default -> throw new IllegalArgumentException(structure);
        };
    }

    private static Stats stats(List<Double> values) {
        double mean = values.stream()
                .mapToDouble(Double::doubleValue)
                .average()
                .orElse(0.0);

        double variance = 0.0;

        for (double value : values) {
            variance += Math.pow(value - mean, 2);
        }

        double std = values.size() > 1
                ? Math.sqrt(variance / (values.size() - 1))
                : 0.0;

        return new Stats(mean, std);
    }

    private static void printTiming(String name, List<Double> times, String unit) {
        Stats s = stats(times);

        System.out.printf(
                Locale.US,
                "%-12s | %.6f %s | desviación: %.6f %s%n",
                name, s.mean(), unit, s.std(), unit
        );
    }

    private static Storage storage(Path path) throws IOException {
        long bytes = 0;
        long files = 0;
        long directories = 0;

        try (Stream<Path> stream = Files.walk(path)) {
            for (Path entry : stream.toList()) {
                if (Files.isRegularFile(entry)) {
                    files++;
                    bytes += Files.size(entry);
                } else if (Files.isDirectory(entry) && !entry.equals(path)) {
                    directories++;
                }
            }
        }

        return new Storage(
                bytes / (1024.0 * 1024.0),
                files,
                directories
        );
    }

    private static void benchmarkWrite(List<Book> books) throws IOException {
        System.out.println("\n========== WRITE / THROUGHPUT ==========");

        for (String structure : STRUCTURES) {
            List<Double> times = new ArrayList<>();

            for (int repetition = 0; repetition < REPETITIONS; repetition++) {
                cleanStructure(structure);

                long start = System.nanoTime();
                saveBooks(structure, books);
                double elapsed = (System.nanoTime() - start) / 1e9;

                times.add(elapsed);
            }

            printTiming(structure, times, "s");

            System.out.printf(
                    Locale.US,
                    "%-12s   %.2f libros/s%n",
                    "",
                    books.size() / stats(times).mean()
            );
        }
    }

    private static void benchmarkLookup(List<Book> books) throws IOException {
        System.out.println("\n========== LOOKUP ==========");

        cleanAll();
        saveAllBooks(books);

        for (String structure : STRUCTURES) {
            List<Double> times = new ArrayList<>();

            for (int repetition = 0; repetition < REPETITIONS; repetition++) {
                long start = System.nanoTime();

                for (int i = 0; i < LOOKUP_REPETITIONS; i++) {
                    for (Book book : books) {
                        Path result = lookup(structure, book.id());

                        if (result == null) {
                            throw new IOException("Libro no encontrado: " + book.id());
                        }
                    }
                }

                double elapsed = (System.nanoTime() - start) / 1e6;
                int searches = LOOKUP_REPETITIONS * books.size();

                times.add(elapsed / searches);
            }

            printTiming(structure, times, "ms/búsqueda");
        }
    }

    private static void benchmarkIncremental(List<Book> books) throws IOException {
        if (books.size() < 10) {
            return;
        }

        List<Book> initial = books.subList(0, books.size() - 10);
        List<Book> added = books.subList(books.size() - 10, books.size());

        System.out.println("\n========== INCREMENTAL INSERTION ==========");
        System.out.println("Libros iniciales: " + initial.size());
        System.out.println("Libros añadidos: " + added.size());
        System.out.println("IDs añadidos: " + added.stream().map(Book::id).toList());

        for (String structure : STRUCTURES) {
            List<Double> times = new ArrayList<>();

            for (int repetition = 0; repetition < REPETITIONS; repetition++) {
                cleanStructure(structure);
                saveBooks(structure, initial);

                long start = System.nanoTime();
                saveBooks(structure, added);
                times.add((System.nanoTime() - start) / 1e9);
            }

            printTiming(structure, times, "s");
        }
    }

    private static Set<String> discoverDownloadedIds(String structure)
            throws IOException {

        Set<String> ids = new HashSet<>();
        Path base = BENCHMARK_PATH.resolve(structure);

        try (Stream<Path> stream = Files.walk(base)) {
            stream.filter(path -> Files.isRegularFile(path)
                            && path.getFileName().toString().endsWith(".body.txt"))
                    .forEach(path -> ids.add(
                            path.getFileName().toString().split("\\.")[0]
                    ));
        }

        return ids;
    }

    private static List<String> discoverPendingBooks(
            String structure,
            Set<String> indexedIds
    ) throws IOException {

        Set<String> downloaded = discoverDownloadedIds(structure);
        downloaded.removeAll(indexedIds);

        return downloaded.stream()
                .sorted(Comparator.comparingInt(Integer::parseInt))
                .toList();
    }

    private static void benchmarkDiscovery(List<Book> books) throws IOException {
        if (books.size() < 10) {
            return;
        }

        Set<String> indexedIds = new HashSet<>();

        for (Book book : books.subList(0, books.size() - 10)) {
            indexedIds.add(book.id());
        }

        List<String> expectedIds = books
                .subList(books.size() - 10, books.size())
                .stream()
                .map(Book::id)
                .sorted(Comparator.comparingInt(Integer::parseInt))
                .toList();

        System.out.println("\n========== PENDING BOOK DISCOVERY ==========");
        System.out.println("Libros descargados: " + books.size());
        System.out.println("Libros indexados: " + indexedIds.size());
        System.out.println("IDs pendientes esperados: " + expectedIds);

        for (String structure : STRUCTURES) {
            cleanStructure(structure);
            saveBooks(structure, books);

            List<Double> times = new ArrayList<>();

            for (int repetition = 0; repetition < REPETITIONS; repetition++) {
                long start = System.nanoTime();

                List<String> result = discoverPendingBooks(
                        structure,
                        indexedIds
                );

                times.add((System.nanoTime() - start) / 1e9);

                if (!result.equals(expectedIds)) {
                    throw new IOException(
                            structure + ": IDs pendientes incorrectos"
                    );
                }
            }

            printTiming(structure, times, "s");
            System.out.println("             IDs: " + expectedIds);
        }
    }

    private static void benchmarkStorage(List<Book> books) throws IOException {
        System.out.println("\n========== STORAGE ==========");

        cleanAll();
        saveAllBooks(books);

        for (String structure : STRUCTURES) {
            Storage s = storage(BENCHMARK_PATH.resolve(structure));

            System.out.printf(
                    Locale.US,
                    "%-12s | %.2f MB | %d archivos | %d directorios%n",
                    structure,
                    s.megabytes(),
                    s.files(),
                    s.directories()
            );
        }
    }

    private static void benchmarkRecovery(List<Book> books) throws IOException {
        if (books.isEmpty()) {
            return;
        }

        Book interrupted = books.get(0);
        Set<String> expectedIds = new HashSet<>();

        for (Book book : books) {
            expectedIds.add(book.id());
        }

        System.out.println("\n========== RECOVERY AFTER INTERRUPTION ==========");
        System.out.println("Libro interrumpido: " + interrupted.id());

        for (String structure : STRUCTURES) {
            cleanStructure(structure);
            saveBooks(structure, books);

            Path bodyPath = lookup(structure, interrupted.id());

            if (bodyPath == null) {
                throw new IOException("Body no encontrado");
            }

            Path headerPath = bodyPath.getParent()
                    .resolve(interrupted.id() + ".header.txt");

            Files.delete(bodyPath);

            if (!Files.isRegularFile(headerPath) || Files.exists(bodyPath)) {
                throw new IOException("Estado de interrupción incorrecto");
            }

            long start = System.nanoTime();
            saveBook(structure, interrupted);
            double elapsed = (System.nanoTime() - start) / 1e9;

            Path base = BENCHMARK_PATH.resolve(structure);
            List<String> headerIds = new ArrayList<>();
            List<String> bodyIds = new ArrayList<>();

            try (Stream<Path> stream = Files.walk(base)) {
                for (Path path : stream.filter(Files::isRegularFile).toList()) {
                    String filename = path.getFileName().toString();

                    if (filename.endsWith(".header.txt")) {
                        headerIds.add(filename.split("\\.")[0]);
                    } else if (filename.endsWith(".body.txt")) {
                        bodyIds.add(filename.split("\\.")[0]);
                    }
                }
            }

            Set<String> headerSet = new HashSet<>(headerIds);
            Set<String> bodySet = new HashSet<>(bodyIds);

            boolean valid = headerSet.equals(expectedIds)
                    && bodySet.equals(expectedIds)
                    && headerIds.size() == expectedIds.size()
                    && bodyIds.size() == expectedIds.size();

            System.out.printf(
                    Locale.US,
                    "%-12s | %.6f s | headers: %d | bodies: %d | pares: %d | %s%n",
                    structure,
                    elapsed,
                    headerIds.size(),
                    bodyIds.size(),
                    headerSet.size(),
                    valid ? "OK" : "ERROR"
            );

            if (!valid) {
                throw new IOException("Recovery incorrecta: " + structure);
            }
        }
    }

    private static void benchmarkScalability(List<Book> books) throws IOException {
        System.out.println("\n========== SCALABILITY ==========");

        int[] sizes = {10, 25, 50, 100};

        List<String> csv = new ArrayList<>();
        csv.add(
                "structure,books,write_mean_s,write_std_s,"
                        + "throughput_books_s,lookup_mean_ms,lookup_std_ms,"
                        + "storage_mb,files,directories"
        );

        for (int size : sizes) {
            if (size > books.size()) {
                continue;
            }

            List<Book> selected = books.subList(0, size);

            System.out.println("\n========== CORPUS: " + size + " BOOKS ==========");

            for (String structure : STRUCTURES) {
                List<Double> writeTimes = new ArrayList<>();
                List<Double> lookupTimes = new ArrayList<>();

                cleanStructure(structure);
                saveBooks(structure, selected);
                cleanStructure(structure);

                for (int repetition = 0; repetition < REPETITIONS; repetition++) {
                    cleanStructure(structure);

                    long start = System.nanoTime();
                    saveBooks(structure, selected);
                    writeTimes.add((System.nanoTime() - start) / 1e9);
                }

                for (Book book : selected) {
                    if (lookup(structure, book.id()) == null) {
                        throw new IOException("Libro no encontrado: " + book.id());
                    }
                }

                for (int repetition = 0; repetition < REPETITIONS; repetition++) {
                    long start = System.nanoTime();

                    for (int i = 0; i < LOOKUP_REPETITIONS; i++) {
                        for (Book book : selected) {
                            if (lookup(structure, book.id()) == null) {
                                throw new IOException(
                                        "Libro no encontrado: " + book.id()
                                );
                            }
                        }
                    }

                    double elapsed = (System.nanoTime() - start) / 1e6;
                    lookupTimes.add(
                            elapsed / (LOOKUP_REPETITIONS * selected.size())
                    );
                }

                Stats write = stats(writeTimes);
                Stats lookup = stats(lookupTimes);
                Storage disk = storage(BENCHMARK_PATH.resolve(structure));

                System.out.printf(
                        Locale.US,
                        "%-12s | escritura: %.6f s | búsqueda: %.6f ms | %.2f MB%n",
                        structure,
                        write.mean(),
                        lookup.mean(),
                        disk.megabytes()
                );

                csv.add(String.format(
                        Locale.US,
                        "%s,%d,%.6f,%.6f,%.2f,%.6f,%.6f,%.4f,%d,%d",
                        structure,
                        size,
                        write.mean(),
                        write.std(),
                        size / write.mean(),
                        lookup.mean(),
                        lookup.std(),
                        disk.megabytes(),
                        disk.files(),
                        disk.directories()
                ));
            }
        }

        Files.createDirectories(RESULTS_PATH);
        Files.write(
                RESULTS_PATH.resolve("java_datalake_scalability.csv"),
                csv,
                StandardCharsets.UTF_8
        );
    }

    public static void main(String[] args) throws Exception {
        List<Book> books = loadBooks();

        System.out.println("========================================");
        System.out.println("       DATALAKE BENCHMARK - JAVA");
        System.out.println("========================================");
        System.out.println("Libros cargados: " + books.size());
        System.out.println("Repeticiones: " + REPETITIONS);
        System.out.println("Sample data: " + SAMPLE_DATA_PATH);
        System.out.println("Benchmark path: " + BENCHMARK_PATH);

        if (books.isEmpty()) {
            System.out.println("No se encontraron libros en sample_data.");
            return;
        }

        benchmarkWrite(books);
        benchmarkLookup(books);
        benchmarkIncremental(books);
        benchmarkDiscovery(books);
        benchmarkStorage(books);
        benchmarkRecovery(books);
        benchmarkScalability(books);

        System.out.println("\n========================================");
        System.out.println("          BENCHMARK COMPLETADO");
        System.out.println("========================================");
    }
}
