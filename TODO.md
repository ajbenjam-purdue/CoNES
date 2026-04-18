## CoNES project goals
 * Incorporate both `routine`s and `function`s:
   * A `routine` should act as a simple copy-paste code block with replaced input and output variables (no return, think of a C function with pointers)
   * A `function` should act as a "compiled" one-item return for varying complexity repeated operations, where a user might create a set of functions in a header and re-use the header
 * Expand/automate the creation of binary tables to include CoolProp's wide array of refrigerants and other substances while providing CLI interface to the python script using cnes.exe/cnes.o
 * Tabular user inputs (arrays) with automatic execution; need to walk a fine line between an eng. tool and a cs tool
 * Python ext tooling for latex rendering/GUI interface