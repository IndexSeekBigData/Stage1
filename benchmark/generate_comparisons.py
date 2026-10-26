from pathlib import Path
import csv
import re
import unicodedata

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
BENCH = ROOT / "benchmark"
FIG = ROOT / "docs" / "figures"

FILES = {
    ("datalake", "Python"): "datalake_raw_output.txt",
    ("datalake", "Java"): "java_datalake_raw_output.txt",
    ("datalake", "C"): "c_datalake_results.txt",
    ("index", "Python"): "index_raw_output.txt",
    ("index", "Java"): "java_index_raw_output.txt",
    ("index", "C"): "c_index_results.txt",
}

LANG = ("Python", "Java", "C")

STRUCT = {
    "datalake": ("time_based", "book_based", "range_based"),
    "index": ("monolithic", "term_files", "hierarchical"),
}

METRICS = {
    "datalake": [
        ("write", "Escritura de 100 libros", "s"),
        ("lookup", "Búsqueda de libros", "ms"),
        ("incremental", "Inserción incremental de 10 libros", "s"),
        ("pending", "Detección de libros pendientes", "s"),
        ("storage", "Almacenamiento", "MB"),
    ],
    "index": [
        ("build", "Construcción del índice", "s"),
        ("query", "Consulta del índice", "ms"),
        ("incremental", "Actualización incremental", "s"),
        ("rebuild", "Reconstrucción completa", "s"),
        ("storage", "Almacenamiento", "MB"),
    ],
}

NUM = r"(\d+(?:[.,]\d+)?)"


# ============================================================
# LECTURA DE ARCHIVOS
# ============================================================

def read_text(path):

    raw = path.read_bytes()

    # Detectar UTF-16 mediante BOM
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        return raw.decode("utf-16")

    # Detectar UTF-8 mediante BOM
    if raw.startswith(b"\xef\xbb\xbf"):
        return raw.decode("utf-8-sig")

    # Probar UTF-8
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        pass

    # Codificaciones alternativas
    for encoding in ("cp1252", "latin-1"):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue

    raise ValueError(f"No se pudo leer el archivo: {path}")


def normalize(s):
    s = unicodedata.normalize("NFKD", s.lower())

    return "".join(
        c for c in s
        if not unicodedata.combining(c)
    )


def blocks(text):

    # Normalizar saltos de línea
    text = text.replace("\r\n", "\n").replace("\r", "\n")

    # Detectar encabezados delimitados por signos =
    pattern = re.compile(
        r"^[ \t]*={4,}[ \t]*([^=\n]+?)[ \t]*={4,}[ \t]*$",
        re.MULTILINE
    )

    matches = list(pattern.finditer(text))

    result = []

    for i, match in enumerate(matches):

        title = match.group(1).strip().upper()

        start = match.end()

        end = (
            matches[i + 1].start()
            if i + 1 < len(matches)
            else len(text)
        )

        result.append(
            (title, text[start:end])
        )

    return result


def get_block(text, title):
    for heading, body in blocks(text):
        if heading == title:
            return body

    raise ValueError(f"Falta sección {title}")


def get_lines(block, structures):
    found = {}

    pattern = re.compile(
        r"^\s*("
        + "|".join(structures)
        + r")\s*\|(.+)$"
    )

    for line in block.splitlines():
        match = pattern.match(line)

        if match:
            found[match.group(1)] = match.group(2)

    if len(found) != len(structures):
        raise ValueError(
            f"Faltan estructuras: "
            f"{set(structures) - set(found)}"
        )

    return found


def value_std(line):
    parts = line.split("|")

    match = re.search(
        NUM + r"\s*(?:ms|s|MB)\b",
        parts[0],
        re.IGNORECASE
    )

    if not match:
        raise ValueError(
            f"Valor no reconocido: {line}"
        )

    value = float(
        match.group(1).replace(",", ".")
    )

    std = None

    if len(parts) > 1:
        match = re.search(
            NUM + r"\s*(?:ms|s)\b",
            parts[1],
            re.IGNORECASE
        )

        if match:
            std = float(
                match.group(1).replace(",", ".")
            )

    return value, std


# ============================================================
# EXTRACCIÓN DE RESULTADOS
# ============================================================

def parse_simple(text, component):

    if component == "datalake":
        titles = {
            "write": "WRITE / THROUGHPUT",
            "lookup": "LOOKUP",
            "incremental": "INCREMENTAL INSERTION",
            "pending": "PENDING BOOK DISCOVERY",
            "storage": "STORAGE",
        }

    else:
        titles = {
            "build": "FULL INDEX BUILD",
            "query": "QUERY PERFORMANCE",
            "storage": "STORAGE",
        }

    out = {}

    for metric, title in titles.items():

        lines = get_lines(
            get_block(text, title),
            STRUCT[component]
        )

        for structure, line in lines.items():
            out[(metric, structure)] = value_std(line)

    return out


def parse_incremental_index(text):

    body = get_block(
        text,
        "INCREMENTAL VS FULL REBUILD"
    )

    out = {}
    structures = STRUCT["index"]

    pattern = re.compile(
        r"^("
        + "|".join(structures)
        + r")\s*$",
        re.MULTILINE
    )

    matches = list(pattern.finditer(body))

    for i, match in enumerate(matches):

        structure = match.group(1)

        segment = body[
                  match.end():
                  matches[i + 1].start()
                  if i + 1 < len(matches)
                  else len(body)
                  ]

        for metric, label in [
            ("incremental", "incremental"),
            ("rebuild", "completa"),
        ]:

            line = next(
                (
                    line
                    for line in segment.splitlines()
                    if label in normalize(line)
                       and ":" in line
                ),
                None
            )

            if line is None:
                raise ValueError(
                    f"Falta {metric} para {structure}"
                )

            value_match = re.search(
                NUM
                + r"\s*s\s*\(std:\s*"
                + NUM,
                line,
                re.IGNORECASE
            )

            if not value_match:
                raise ValueError(
                    f"Formato incremental inesperado: {line}"
                )

            out[(metric, structure)] = tuple(
                float(v.replace(",", "."))
                for v in value_match.groups()
            )

    if len(out) != 6:
        raise ValueError(
            "No se encontraron las seis medidas incrementales"
        )

    return out


def parse_scalability(text):

    out = {}
    sections = blocks(text)

    for heading, body in sections:

        match = re.fullmatch(
            r"CORPUS:\s*(\d+)\s*BOOKS",
            heading
        )

        if not match:
            continue

        count = int(match.group(1))

        # Admite:
        # Python: bloques multilínea
        # Java/C: líneas compactas

        pattern = re.compile(
            r"^\s*("
            + "|".join(STRUCT["datalake"])
            + r")\s*(?:\|([^\r\n]*))?\s*$",
            re.MULTILINE
        )

        found = list(pattern.finditer(body))

        for i, structure_match in enumerate(found):

            structure = structure_match.group(1)

            segment = body[
                      structure_match.start():
                      found[i + 1].start()
                      if i + 1 < len(found)
                      else len(body)
                      ]

            normalized = normalize(segment)

            write = re.search(
                r"escritura:\s*"
                + NUM
                + r"\s*s",
                normalized
            )

            # Admite búsqueda, busqueda y B·squeda
            lookup = re.search(
                r"b[^:\r\n ]*squeda:\s*"
                + NUM
                + r"\s*ms",
                normalized
            )

            storage = re.search(
                r"(?:almacenamiento:\s*)?"
                + NUM
                + r"\s*MB",
                segment,
                re.IGNORECASE
            )

            if not (write and lookup and storage):
                raise ValueError(
                    f"Escalabilidad incompleta: "
                    f"{count} libros, {structure}"
                )

            out[(count, structure)] = (
                float(write.group(1)),
                float(lookup.group(1)),
                float(storage.group(1))
            )

    if len(out) != 12:
        raise ValueError(
            f"Se esperaban 12 registros de escalabilidad, "
            f"encontrados {len(out)}"
        )

    return out


# ============================================================
# GENERACIÓN DE CSV
# ============================================================

def save_csv(path, fields, rows):

    with path.open(
            "w",
            newline="",
            encoding="utf-8"
    ) as file:

        writer = csv.DictWriter(
            file,
            fieldnames=fields
        )

        writer.writeheader()
        writer.writerows(rows)


# ============================================================
# GRÁFICAS COMPARATIVAS
# ============================================================

def bar_chart(component, metric, title, unit, results):

    fig, ax = plt.subplots(figsize=(9, 5))

    positions = range(3)

    for i, lang in enumerate(LANG):

        measures = [
            results[(component, lang, metric, structure)]
            for structure in STRUCT[component]
        ]

        values = [
            value[0]
            for value in measures
        ]

        errors = [
            value[1] or 0
            for value in measures
        ]

        ax.bar(
            [
                position + (i - 1) * 0.25
                for position in positions
            ],
            values,
            width=0.25,
            label=lang,
            yerr=errors if any(errors) else None,
            capsize=3
        )

    ax.set_xticks(
        list(positions),
        [
            structure.replace("_", " ").title()
            for structure in STRUCT[component]
        ]
    )

    ax.set_ylabel(unit)
    ax.set_title(
        f"{component.upper()}: {title}"
    )

    ax.grid(
        axis="y",
        alpha=0.2
    )

    ax.set_axisbelow(True)
    ax.legend()

    fig.tight_layout()

    fig.savefig(
        FIG / f"{component}_{metric}.png",
        dpi=180
    )

    plt.close(fig)


def scalability_chart(metric, scale):

    fig, axes = plt.subplots(
        1,
        3,
        figsize=(13, 4),
        sharey=True
    )

    sizes = (10, 25, 50, 100)

    index = {
        "write": 0,
        "lookup": 1
    }[metric]

    for ax, structure in zip(
            axes,
            STRUCT["datalake"]
    ):

        for lang in LANG:

            values = [
                scale[(lang, n, structure)][index]
                for n in sizes
            ]

            ax.plot(
                sizes,
                values,
                marker="o",
                label=lang
            )

        ax.set_title(
            structure.replace("_", " ").title()
        )

        ax.set_xlabel("Libros")
        ax.grid(alpha=0.2)

    axes[0].set_ylabel(
        "s"
        if metric == "write"
        else "ms/búsqueda"
    )

    axes[-1].legend()

    fig.suptitle(
        f"Escalabilidad del datalake: {metric}"
    )

    fig.tight_layout()

    fig.savefig(
        FIG / f"datalake_scalability_{metric}.png",
        dpi=180
    )

    plt.close(fig)


# ============================================================
# PROGRAMA PRINCIPAL
# ============================================================

def main():

    FIG.mkdir(
        parents=True,
        exist_ok=True
    )

    results = {}
    scale = {}

    # --------------------------------------------------------
    # Leer los seis benchmarks
    # --------------------------------------------------------

    for (component, lang), filename in FILES.items():

        text = read_text(
            BENCH / filename
        )

        measures = parse_simple(
            text,
            component
        )

        if component == "index":

            measures.update(
                parse_incremental_index(text)
            )

        else:

            for key, value in parse_scalability(text).items():
                scale[(lang, *key)] = value

        for (metric, structure), values in measures.items():

            results[
                (component, lang, metric, structure)
            ] = values

        print(f"Leído: {filename}")

    # --------------------------------------------------------
    # Preparar tablas comparativas
    # --------------------------------------------------------

    rows = []

    md = [
        "# Resultados comparativos de InverSeek",
        "",
        "Medias de los benchmarks originales. "
        "Menor tiempo y menor espacio son mejores.",
        "",
        "Datalake: 5 repeticiones. "
        "Índice invertido: 3 repeticiones.",
        "",
        "**Limitaciones:** existen diferencias de tamaños "
        "almacenados, vocabulario y algunos IDs entre lenguajes; "
        "las comparaciones son descriptivas y no prueban "
        "igualdad completa de cargas de trabajo. "
        "Los valores de escalabilidad se presentan por separado "
        "de las pruebas principales de 100 libros.",
        ""
    ]

    for component in ("datalake", "index"):

        md += [
            f"## {component.title()}",
            ""
        ]

        for metric, title, unit in METRICS[component]:

            md += [
                f"### {title}",
                "",
                f"| Estructura | Python ({unit}) | "
                f"Java ({unit}) | C ({unit}) |",
                "|---|---:|---:|---:|"
            ]

            for structure in STRUCT[component]:

                vals = []

                for lang in LANG:

                    mean, std = results[
                        (component, lang, metric, structure)
                    ]

                    vals.append(
                        f"{mean:.6f}"
                    )

                    rows.append({
                        "component": component,
                        "metric": metric,
                        "structure": structure,
                        "language": lang,
                        "mean": f"{mean:.6f}",
                        "std": (
                            ""
                            if std is None
                            else f"{std:.6f}"
                        ),
                        "unit": unit
                    })

                md.append(
                    f"| `{structure}` | "
                    + " | ".join(vals)
                    + " |"
                )

            md += [
                "",
                f"![{title}]"
                f"(../docs/figures/{component}_{metric}.png)",
                ""
            ]

            bar_chart(
                component,
                metric,
                title,
                unit,
                results
            )

    save_csv(
        BENCH / "comparative_results.csv",
        [
            "component",
            "metric",
            "structure",
            "language",
            "mean",
            "std",
            "unit"
        ],
        rows
    )

    # --------------------------------------------------------
    # Escalabilidad del datalake
    # --------------------------------------------------------

    scale_rows = []

    md += [
        "## Escalabilidad del datalake",
        "",
        "Resultados específicos de las pruebas "
        "con 10, 25, 50 y 100 libros.",
        ""
    ]

    for metric, idx, unit in [
        ("write", 0, "s"),
        ("lookup", 1, "ms")
    ]:

        md += [
            f"### {metric.title()} ({unit})",
            "",
            "| Libros | Estructura | Python | Java | C |",
            "|---:|---|---:|---:|---:|"
        ]

        for n in (10, 25, 50, 100):

            for structure in STRUCT["datalake"]:

                values = [
                    scale[(lang, n, structure)][idx]
                    for lang in LANG
                ]

                md.append(
                    f"| {n} | `{structure}` | "
                    + " | ".join(
                        f"{value:.6f}"
                        for value in values
                    )
                    + " |"
                )

        md += [
            "",
            f"![Escalabilidad {metric}]"
            f"(../docs/figures/datalake_scalability_{metric}.png)",
            ""
        ]

        scalability_chart(
            metric,
            scale
        )

    for lang in LANG:

        for n in (10, 25, 50, 100):

            for structure in STRUCT["datalake"]:

                write, lookup, mb = scale[
                    (lang, n, structure)
                ]

                scale_rows.append({
                    "language": lang,
                    "books": n,
                    "structure": structure,
                    "write_s": f"{write:.6f}",
                    "lookup_ms": f"{lookup:.6f}",
                    "storage_mb": f"{mb:.2f}"
                })

    save_csv(
        BENCH / "comparative_scalability.csv",
        [
            "language",
            "books",
            "structure",
            "write_s",
            "lookup_ms",
            "storage_mb"
        ],
        scale_rows
    )

    # --------------------------------------------------------
    # Generar RESULTADOS.md
    # --------------------------------------------------------

    output = BENCH / "RESULTADOS.md"

    output.write_text(
        "\n".join(md) + "\n",
        encoding="utf-8"
    )

    print(
        "Generados RESULTADOS.md, dos CSV "
        "y 12 gráficas en docs/figures/"
    )


if __name__ == "__main__":
    main()