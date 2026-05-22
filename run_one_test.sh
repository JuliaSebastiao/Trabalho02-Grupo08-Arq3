#!/usr/bin/env sh
set -eu

print_usage() {
    echo "Uso: sh ./run_one_test.sh <teste> [executavel] [--step|--quiet]"
    echo ""
    echo "Exemplos:"
    echo "  sh ./run_one_test.sh 03"
    echo "  sh ./run_one_test.sh waw"
    echo "  sh ./run_one_test.sh hennessy ./tomasulo --step"
    echo "  sh ./run_one_test.sh --list"
}

description_for() {
    sed -n 's/^# *//p' "$1" | sed -n '1p'
}

list_tests() {
    echo "Testes disponiveis:"
    for file in ./examples/*.txt ./tests/*.txt; do
        [ -e "$file" ] || continue
        name="$(basename "$file")"
        description="$(description_for "$file")"
        printf '%-42s %s\n' "- $name" "$description"
    done
}

if [ "$#" -eq 0 ]; then
    print_usage >&2
    exit 1
fi

case "$1" in
    --list|-l)
        list_tests
        exit 0
        ;;
    --help|-h)
        print_usage
        exit 0
        ;;
esac

test_query="$1"
shift

exe="./tomasulo"
mode=""

if [ "$#" -gt 0 ]; then
    case "$1" in
        --step|--quiet)
            mode="$1"
            shift
            ;;
        *)
            exe="$1"
            shift
            ;;
    esac
fi

if [ "$#" -gt 0 ]; then
    case "$1" in
        --step|--quiet)
            mode="$1"
            shift
            ;;
        *)
            echo "Opcao desconhecida: $1" >&2
            print_usage >&2
            exit 1
            ;;
    esac
fi

if [ "$#" -gt 0 ]; then
    echo "Argumentos extras nao reconhecidos." >&2
    print_usage >&2
    exit 1
fi

if [ ! -x "$exe" ]; then
    echo "Executavel nao encontrado ou sem permissao de execucao: $exe" >&2
    echo "Compile antes com: make" >&2
    exit 1
fi

matches=""
count=0

if [ -f "$test_query" ]; then
    matches="$test_query"
    count=1
else
    needle="$(printf '%s' "$test_query" | tr '[:upper:]' '[:lower:]')"
    numeric=""
    case "$needle" in
        *[!0-9]*) numeric="" ;;
        *) numeric="$needle" ;;
    esac

    for file in ./examples/*.txt ./tests/*.txt; do
        [ -e "$file" ] || continue
        name="$(basename "$file")"
        base="${name%.txt}"
        name_lower="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')"
        base_lower="$(printf '%s' "$base" | tr '[:upper:]' '[:lower:]')"

        found=0
        case "$name_lower" in
            "$needle"|"$needle"*|*"$needle"*) found=1 ;;
        esac
        case "$base_lower" in
            "$needle"|"$needle"*|*"$needle"*) found=1 ;;
        esac
        if [ -n "$numeric" ]; then
            if [ "${#numeric}" -eq 1 ]; then
                prefix="0${numeric}_"
            else
                prefix="${numeric}_"
            fi
            case "$name" in
                "$prefix"*) found=1 ;;
            esac
        fi

        if [ "$found" -eq 1 ]; then
            matches="${matches}${file}
"
            count=$((count + 1))
        fi
    done
fi

if [ "$count" -eq 0 ]; then
    echo "Nenhum teste encontrado para '$test_query'. Use --list para ver as opcoes." >&2
    exit 1
fi

if [ "$count" -gt 1 ]; then
    echo "Mais de um teste combina com '$test_query':" >&2
    printf '%s' "$matches" | while IFS= read -r file; do
        [ -n "$file" ] && echo "- $(basename "$file")" >&2
    done
    echo "Use um nome mais especifico." >&2
    exit 1
fi

selected="$(printf '%s' "$matches" | sed -n '1p')"
echo "Rodando teste: $(basename "$selected")"
echo "Arquivo: $selected"

if [ -n "$mode" ]; then
    "$exe" "$selected" "$mode"
else
    "$exe" "$selected"
fi
