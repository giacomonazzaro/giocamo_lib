find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -not -path "./.git/*" -not -path "*/build/*" -exec clang-format -i {} \;

