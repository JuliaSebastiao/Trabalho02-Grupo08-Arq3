#!/usr/bin/env sh
set -eu

exe="${1:-./tomasulo}"

if [ ! -x "$exe" ]; then
    echo "Executavel nao encontrado ou sem permissao de execucao: $exe" >&2
    echo "Compile antes com: make" >&2
    exit 1
fi

failed=0

run_file() {
    file="$1"
    echo "== $file =="
    if "$exe" "$file" --quiet; then
        echo "PASS: $(basename "$file")"
    else
        echo "FAIL: $(basename "$file")" >&2
        failed=$((failed + 1))
    fi
    echo ""
}

for file in ./examples/*.txt; do
    [ -e "$file" ] || continue
    run_file "$file"
done

for file in ./tests/*.txt; do
    [ -e "$file" ] || continue
    run_file "$file"
done

if [ "$failed" -gt 0 ]; then
    echo "$failed teste(s) falharam." >&2
    exit 1
fi

echo "Todos os testes passaram."
