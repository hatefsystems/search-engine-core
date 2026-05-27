#!/bin/bash
# Download FastText Language Detection Models
# Task 01.2 - Language Detection

set -e  # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
MODELS_DIR="$PROJECT_DIR/models"

echo "========================================="
echo "FastText Language Detection Models"
echo "========================================="
echo ""

# Create models directory
mkdir -p "$MODELS_DIR"
cd "$MODELS_DIR"

echo "📥 Downloading FastText language identification models..."
echo ""

# Option 1: Compressed model (917KB) - Fast, good accuracy
echo "1️⃣  Downloading lid.176.bin (917KB, compressed)..."
if [ -f "lid.176.bin" ]; then
    echo "   ⚠️  lid.176.bin already exists, skipping..."
else
    wget -q --show-progress \
        https://dl.fbaipublicfiles.com/fasttext/supervised-models/lid.176.bin \
        -O lid.176.bin
    echo "   ✅ lid.176.bin downloaded (917KB)"
fi
echo ""

# Option 2: Full model (126MB) - Best accuracy (recommended)
echo "2️⃣  Downloading lid.176.ftz (126MB, full accuracy)..."
if [ -f "lid.176.ftz" ]; then
    echo "   ⚠️  lid.176.ftz already exists, skipping..."
else
    wget -q --show-progress \
        https://dl.fbaipublicfiles.com/fasttext/supervised-models/lid.176.ftz \
        -O lid.176.ftz
    echo "   ✅ lid.176.ftz downloaded (126MB)"
fi
echo ""

# Create custom models directory
mkdir -p custom
echo "📁 Created custom models directory for your trained models"
echo ""

# Summary
echo "========================================="
echo "✅ Download Complete!"
echo "========================================="
echo ""
echo "Available models:"
ls -lh *.bin *.ftz 2>/dev/null | awk '{print "  -", $9, "("$5")"}'
echo ""
echo "📖 Usage:"
echo "  - lid.176.bin: Fast, good for production (917KB)"
echo "  - lid.176.ftz: Best accuracy, recommended (126MB) ✨"
echo "  - custom/*: Your trained models (up to 250+ languages)"
echo ""
echo "🚀 Ready to detect 176 languages!"
echo ""

