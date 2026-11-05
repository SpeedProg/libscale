#!/usr/bin/env python3
import os
import re
import sys

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
DEFAULT_INCLUDE_DIR = os.path.join(REPO_ROOT, "include", "gui")

# Doxygen comment regexes
RE_DOXYGEN_LINE = re.compile(r'^\s*///')
RE_DOXYGEN_BLOCK_START = re.compile(r'^\s*/\*[*!]')

# C++ declaration regexes
RE_CLASS_STRUCT_ENUM = re.compile(r'^\s*(template\s*<.*?>\s*)?(class|struct|enum class|enum)\s+([a-zA-Z0-9_]+)')
RE_FORWARD_DECL = re.compile(r';\s*$')

def is_doxygen_comment(line: str) -> bool:
    return bool(RE_DOXYGEN_LINE.match(line) or RE_DOXYGEN_BLOCK_START.match(line))

def check_file(filepath: str) -> list:
    errors = []
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Pre-calculate which lines are part of Doxygen comments
    in_doxygen_block = False
    has_doxygen_before = [False] * len(lines)
    
    # We scan backwards or forwards to mark lines that have a Doxygen comment immediately preceding them
    # For simplicity, let's track the Doxygen comment presence.
    # A line has a doxygen comment if the preceding non-empty lines are doxygen comments.
    doxygen_buffer = False
    for i in range(len(lines)):
        line = lines[i].strip()
        if not line:
            continue
        if line.startswith('///') or line.startswith('/**') or line.startswith('*'):
            doxygen_buffer = True
        elif line.endswith('*/'):
            doxygen_buffer = True
        else:
            has_doxygen_before[i] = doxygen_buffer
            doxygen_buffer = False

    # Also check direct preceding line logic for better precision
    def has_doc_preceding(idx: int) -> bool:
        # Scan upwards from idx - 1, ignoring empty lines and template prefixes
        j = idx - 1
        while j >= 0:
            l = lines[j].strip()
            if not l:
                j -= 1
                continue
            if l.startswith('template'):
                j -= 1
                continue
            if l.startswith('///') or l.endswith('*/') or l.startswith('*') or l.startswith('/**'):
                return True
            break
        return False

    # Parser state
    in_class = False
    class_name = ""
    visibility = "private" # default for class
    brace_count = 0
    class_brace_level = 0
    
    for i, raw_line in enumerate(lines):
        line = raw_line.strip()
        
        # Track braces to know when we exit a class/struct
        open_braces = line.count('{')
        close_braces = line.count('}')
        
        # Visibility changes
        if in_class and brace_count == class_brace_level + 1:
            if line.startswith('public:'):
                visibility = "public"
                continue
            elif line.startswith('private:') or line.startswith('protected:'):
                visibility = "private"
                continue
 
        # Check for class/struct/enum declarations
        match = RE_CLASS_STRUCT_ENUM.match(raw_line)
        if match and not RE_FORWARD_DECL.search(line):
            decl_type = match.group(2)
            name = match.group(3)
            
            # Check if this class/struct/enum has a Doxygen comment
            if not has_doc_preceding(i):
                errors.append((i + 1, f"Missing documentation for {decl_type} '{name}'"))
            
            if decl_type in ('class', 'struct'):
                in_class = True
                class_name = name
                visibility = "public" if decl_type == 'struct' else "private"
                class_brace_level = brace_count
                
        # Handle brace tracking
        for char in line:
            if char == '{':
                brace_count += 1
            elif char == '}':
                brace_count -= 1
                if in_class and brace_count <= class_brace_level:
                    in_class = False
                    class_name = ""
 
        # Check for public methods inside class
        if in_class and visibility == "public" and brace_count == class_brace_level + 1:
            # We are in the public section of the class. Identify potential function/method declarations
            # Simple heuristic: must contain '(' and end with ';' or '{' or 'override' or 'const', but not be a constructor helper or operators if deleted/defaulted
            if '(' in line and not line.startswith('//') and not line.startswith('/*'):
                # Ignore deleted/defaulted functions or common operators if they are simple
                if '= delete' in line or '= default' in line:
                    continue
                # Ignore constructor declarations that are deleted
                if class_name in line and '= delete' in line:
                    continue
                # Ignore virtual method overrides as they inherit documentation from the interface
                if 'override' in line:
                    continue
                
                # Check for Doxygen documentation
                if not has_doc_preceding(i):
                    # Extract a clean function signature for printing
                    sig = line.split('{')[0].strip().rstrip(';')
                    errors.append((i + 1, f"Missing documentation for public member in '{class_name}': {sig}"))

    return errors

def main():
    include_dir = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_INCLUDE_DIR
    if not os.path.isdir(include_dir):
        print(f"Error: Directory {include_dir} not found.")
        sys.exit(1)

    # Recursively find all .hpp files in the include directory
    target_files = []
    for root, _, files in os.walk(include_dir):
        for file in files:
            if file.endswith('.hpp'):
                target_files.append(os.path.join(root, file))

    target_files.sort()

    total_errors = 0
    for filepath in target_files:
        errors = check_file(filepath)
        if errors:
            rel_path = os.path.relpath(filepath, REPO_ROOT)
            print(f"\n[{rel_path}]")
            for line_num, err in errors:
                print(f"  Line {line_num}: {err}")
            total_errors += len(errors)

    if total_errors > 0:
        print(f"\nTotal documentation violations found: {total_errors}")
        sys.exit(1)
    else:
        print("\nAll public classes, structs, enums, and methods are properly documented!")
        sys.exit(0)

if __name__ == "__main__":
    main()
