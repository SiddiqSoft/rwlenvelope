#!/bin/bash
# Generate Doxygen documentation for asynchrony library
# This script generates the HTML documentation locally

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo -e "${YELLOW}Asynchrony Library - Documentation Generator${NC}"
echo "=================================================="
echo ""

# Check if Doxygen is installed
if ! command -v doxygen &> /dev/null; then
    echo -e "${RED}Error: Doxygen is not installed${NC}"
    echo ""
    echo "Please install Doxygen:"
    echo "  macOS:  brew install doxygen graphviz"
    echo "  Ubuntu: sudo apt-get install doxygen graphviz"
    echo "  Windows: Download from https://www.doxygen.nl/download.html"
    exit 1
fi

# Check if Graphviz is installed (optional but recommended)
if ! command -v dot &> /dev/null; then
    echo -e "${YELLOW}Warning: Graphviz is not installed${NC}"
    echo "Diagrams will not be generated. Install with:"
    echo "  macOS:  brew install graphviz"
    echo "  Ubuntu: sudo apt-get install graphviz"
    echo ""
fi

# Print versions
echo "Doxygen version:"
doxygen --version
echo ""

# Check if Doxyfile exists
if [ ! -f "$SCRIPT_DIR/Doxyfile" ]; then
    echo -e "${RED}Error: Doxyfile not found in $SCRIPT_DIR${NC}"
    exit 1
fi

# Clean previous build
echo -e "${YELLOW}Cleaning previous documentation...${NC}"
rm -rf "$SCRIPT_DIR/docs/doxygen"
echo -e "${GREEN}✓ Cleaned${NC}"
echo ""

# Generate documentation
echo -e "${YELLOW}Generating documentation...${NC}"
cd "$SCRIPT_DIR"
doxygen Doxyfile

# Check if generation was successful
if [ -d "$SCRIPT_DIR/docs/doxygen/html" ]; then
    echo -e "${GREEN}✓ Documentation generated successfully${NC}"
    echo ""
    
    # Count generated files
    FILE_COUNT=$(find "$SCRIPT_DIR/docs/doxygen/html" -type f | wc -l)
    echo "Generated $FILE_COUNT files"
    echo ""
    
    # Print location
    echo -e "${GREEN}Documentation location:${NC}"
    echo "  $SCRIPT_DIR/docs/doxygen/html/"
    echo ""
    
    # Offer to open in browser
    if command -v open &> /dev/null; then
        echo -e "${YELLOW}Opening documentation in browser...${NC}"
        open "$SCRIPT_DIR/docs/doxygen/html/index.html"
    elif command -v xdg-open &> /dev/null; then
        echo -e "${YELLOW}Opening documentation in browser...${NC}"
        xdg-open "$SCRIPT_DIR/docs/doxygen/html/index.html"
    else
        echo -e "${YELLOW}To view documentation, open:${NC}"
        echo "  $SCRIPT_DIR/docs/doxygen/html/index.html"
    fi
else
    echo -e "${RED}✗ Documentation generation failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}Done!${NC}"
