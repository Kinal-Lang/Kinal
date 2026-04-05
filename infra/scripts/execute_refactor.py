import os
import shutil
from pathlib import Path

ROOT = Path("k:\\Project\\Kinal-Lang")

def replace_in_file(path, old, new):
    if not path.exists():
        return
    text = path.read_text(encoding="utf-8")
    if old in text:
        text = text.replace(old, new)
        path.write_text(text, encoding="utf-8")
        print(f"Updated {path.name}")

def move_dir(old_name, new_path):
    old = ROOT / old_name
    new = ROOT / new_path
    if old.exists():
        new.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(old), str(new))
        print(f"Moved {old_name} -> {new_path}")

def main():
    # Move directories
    move_dir("kinal-compiler", "compiler/knc")
    move_dir("kinal-vm", "compiler/knvm")
    move_dir("kinal-lsp", "compiler/kn-lsp")
    move_dir("selfhost", "compiler/selfhost")
    move_dir("stdpkg", "library/std")
    move_dir("runtime", "library/runtime")
    
    # Update root CMakeLists.txt
    root_cmake = ROOT / "CMakeLists.txt"
    replace_in_file(root_cmake, 'add_subdirectory(kinal-compiler)', 'add_subdirectory(compiler/knc)')
    replace_in_file(root_cmake, 'add_subdirectory(kinal-lsp/server)', 'add_subdirectory(compiler/kn-lsp/server)')
    
    # Update compiler/knc/CMakeLists.txt
    knc_cmake = ROOT / "compiler" / "knc" / "CMakeLists.txt"
    replace_in_file(knc_cmake, '../runtime/src/', '../../library/runtime/src/')
    replace_in_file(knc_cmake, '../runtime/include', '../../library/runtime/include')
    
    # Update compiler/kn-lsp/server/CMakeLists.txt
    lsp_cmake = ROOT / "compiler" / "kn-lsp" / "server" / "CMakeLists.txt"
    replace_in_file(lsp_cmake, '../../kinal-compiler', '../../knc')
    replace_in_file(lsp_cmake, '../../runtime', '../../../library/runtime')
    
    # Update x.py
    x_py = ROOT / "x.py"
    replace_in_file(x_py, 'ROOT / "kinal-lsp"', 'ROOT / "compiler" / "kn-lsp"')
    replace_in_file(x_py, 'ROOT / "runtime"', 'ROOT / "library" / "runtime"')
    replace_in_file(x_py, 'ROOT / "stdpkg"', 'ROOT / "library" / "std"')
    replace_in_file(x_py, 'ROOT / "selfhost"', 'ROOT / "compiler" / "selfhost"')
    
    print("Refactoring complete.")

if __name__ == "__main__":
    main()
