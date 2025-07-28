#!/bin/bash
cd /Users/ivan/Github/mTroll/build
export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
export Qt5_DIR="/opt/homebrew/opt/qt@5/lib/cmake/Qt5"
make -j4
echo "Build completed. Binary location:"
find . -name "mTroll" -type f