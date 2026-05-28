#!/bin/bash

# Script para construir o índice HNSW
# Uso: ./build_index.sh [porcentagem]
# Exemplo: ./build_index.sh 0.1 (10% dos dados)
#          ./build_index.sh 1.0 (100% dos dados)

PERCENTAGE=${1:-1.0}

# Escolher arquivo baseado na porcentagem
if (( $(echo "$PERCENTAGE < 0.2" | bc -l) )); then
    INPUT_FILE="data/references_300k.bin"
    echo "Usando subset 10% (300k registros)"
else
    INPUT_FILE="data/references.bin"
    echo "Usando dataset completo (3M registros)"
fi

OUTPUT_FILE="data/hnsw_index.bin"

# Compilar se necessário
if [ ! -f "build_index_from_bin" ] && [ ! -f "build_index_from_bin.exe" ]; then
    echo "Compilando build_index_from_bin..."
    g++ -O3 -std=c++20 tools/build_index_from_bin.cpp -I. -Ihnswlib -o build_index_from_bin
fi

# Executar
echo "Construindo índice: $INPUT_FILE -> $OUTPUT_FILE"
./build_index_from_bin "$INPUT_FILE" "$OUTPUT_FILE"

echo "Índice criado com sucesso!"
