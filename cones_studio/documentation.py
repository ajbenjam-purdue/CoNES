from dataclasses import dataclass
import re

# Pattern for matching comments + funct name (not used)
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

# This is pretty bad but it works
def squish_tuple(x:tuple) -> tuple[str,str]:
    return tuple([i for i in x][0:2])

@dataclass
class Documentation():
    """
    Data class for containing documentation info
    """
    
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
        """Basic formatting, SIGNATURE (ARGUMENTS) -> RETURN [RETURN TYPE]

        Returns:
            str: _description_
        """
        return f"{self.SIGNATURE} ({', '.join([param for param, desc in self.PARAMETERS])}) -> {self.RETURN[0][0:40]} [{self.RETURN[1]}]"
    
    def is_complete(self) -> bool:
        return bool(self.DESCRIPTION)
    
    def to_dict(self) -> dict[str,dict[str,str]]:
        if self.is_complete(): return {self.SIGNATURE : {'sig': f"{self.SIGNATURE} ({", ".join([arg for arg, des in self.PARAMETERS])})", 'desc': f"{self.DESCRIPTION}:{self.RETURN[0]}[{self.RETURN[1]}]"}}
        return {}
    
    @classmethod
    def from_text(cls, text:str) -> Documentation:
        """
        Returns a Documentation object based on a chunk of text (which should be the comments preceeding a function declaration)
        """
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
        """
        Returns a list of Documentation objects based on the contents of a block of text; this will scrape for real functions with preceeding comments
        """
        return [cls.from_text(match['comments']) for match in re.finditer(pattern, text_block)]
    
    @classmethod
    def from_file(cls, path:str) -> list[Documentation]:
        """
        Returns a list of Documentation objects based on the contents of a file; this will scrape for real functions with preceeding comments
        """
        with open(path, 'r') as f:
            return cls.from_text_block(f.read())
        
    def __eq__(self, value: object) -> bool:
        if not isinstance(value, Documentation): return False
        return self.DESCRIPTION == value.DESCRIPTION and \
            self.PARAMETERS == value.PARAMETERS and \
            self.SIGNATURE == value.SIGNATURE and \
            self.RETURN == value.RETURN
    
    def __hash__(self) -> int:
        return hash((self.DESCRIPTION, tuple(self.PARAMETERS), self.SIGNATURE, self.RETURN))