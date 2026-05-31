from typing import Any
import re

pattern = re.compile(
    r"""
    (?P<comments>
        (?:
            ^[ \t]*//.*\n
        )+
    )
    ^[ \t]*function[ \t]+(?P<function_name>\w+)
    """,
    re.MULTILINE | re.VERBOSE,
)

abs_path = "/Users/ajbenj/Documents/GitHub/CoNES/libs/fins.cnes"

# This is pretty bad but it works
def squish_tuple(x:tuple) -> tuple[str,str]:
    return tuple([i for i in x][0:2])

class Documentation():
    # 
    
    def __init__(self, SIGNATURE:str="", PARAMETERS:list[tuple[str,str]]=[], RETURN:tuple[str,str]=("", ""), DESCRIPTION:str="") -> None:
        """Create an instance of a Documentation object, which contains a `SIGNATURE` *(like `my_function`)*, 
        a list of `PARAMETERS` where each has a name and a brief description *(like `arg_1`, `The first argument of the function`)*, 
        a `RETURN` tuple with the plaintext description of the return and the expected units *(like `Calculated Temperature`, `K`)*, 
        and a plaintext `DESCRIPTION`.<br>

        Args:
            SIGNATURE (str, optional): _The function or routine call_. Defaults to "".
            PARAMETERS (list[tuple[str,str]], optional): _A list of tuples in the format ("code_name", "Brief description")_. Defaults to [].
            RETURN (tuple[str,str], optional): _A tuple in the format ("Return description", "Return units")_. Defaults to ("", "").
            DESCRIPTION (str, optional): _A plaintext description of what the function/routine does or how it works_. Defaults to "".
        """
        self.SIGNATURE:str = SIGNATURE
        self.PARAMETERS:list[tuple[str,str]] = PARAMETERS
        self.RETURN:tuple[str,str] = RETURN
        self.DESCRIPTION:str = DESCRIPTION
    
    def __str__(self) -> str:
        return f"{self.SIGNATURE} ({', '.join([param for param, desc in self.PARAMETERS])}) -> {self.RETURN[0][0:40]} [{self.RETURN[1]}]"
    
    @classmethod
    def from_text(cls, text:str) -> Documentation:
        SIGNATURE:str = ""
        PARAMETERS:list[tuple[str,str]] = []
        RETURN:tuple[str,str] = ("", "")
        DESCRIPTION:str = ""
        for line in [line.strip("//").strip() for line in text.split("\n")]:
            if   "`SIGNATURE`" in line: SIGNATURE = line.strip('`SIGNATURE`').strip()
            elif "`PARAMETERS`" in line: 
                PARAMETERS = [squish_tuple(re.match(r'(.+?)\s*\((.+)\)', item).groups()) for item in line.strip('`PARAMETERS`').strip().split(',') if re.match(r'(.+?)\s*\((.+)\)', item)] # type: ignore
            elif "`RETURN`" in line: 
                inter = line.strip('`RETURN`').strip().split(',')
                RETURN = (inter[0].strip() if len(inter) > 0 else "", inter[1].strip() if len(inter) > 1 else "")
            elif "`DESCRIPTION`" in line: DESCRIPTION = line.strip('`DESCRIPTION`').strip()
        return cls(SIGNATURE, PARAMETERS, RETURN, DESCRIPTION)
    
    @classmethod
    def from_text_block(cls, text_block:str) -> list[Documentation]:
        return [cls.from_text(match['comments']) for match in re.finditer(pattern, text_block)]
    
    @classmethod
    def from_file(cls, path:str) -> list[Documentation]:
        with open(path, 'r') as f:
            return cls.from_text_block(f.read())

print("\n\n".join([str(x) for x in Documentation.from_file(abs_path)]))