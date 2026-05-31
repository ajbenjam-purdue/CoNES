from dataclasses import dataclass, field
from typing import Optional, List, Tuple, Dict, Any
import re

# Pattern for matching comments + function name
FUNCTION_PATTERN = re.compile(
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

@dataclass(frozen=True)
class Documentation:
    """
    Data class representing CoNES function documentation extracted from comments.
    """
    signature: str = ""
    parameters: List[Tuple[str, str]] = field(default_factory=list)
    return_info: Tuple[str, str] = ("", "")
    description: str = ""

    def __str__(self) -> str:
        """Standard representation: signature (args) -> return [units]."""
        params_str = ", ".join([p[0] for p in self.parameters])
        ret_desc = self.return_info[0][:40]
        ret_units = self.return_info[1]
        return f"{self.signature} ({params_str}) -> {ret_desc} [{ret_units}]"

    def is_complete(self) -> bool:
        """Returns True if the documentation has a description."""
        return bool(self.description)

    def to_dict(self) -> Dict[str, Dict[str, str]]:
        """Converts the documentation to a dictionary for metadata/autocomplete."""
        if not self.is_complete():
            return {}

        args_str = ", ".join([arg for arg, _ in self.parameters])
        return {
            self.signature: {
                'sig': f"{self.signature} ({args_str})",
                'desc': f"{self.description}:{self.return_info[0]}[{self.return_info[1]}]"
            }
        }

    @classmethod
    def from_text(cls, text: str) -> 'Documentation':
        """Parses a block of comments into a Documentation object."""
        signature = ""
        parameters = []
        return_info = ("", "")
        description = ""

        # Strip // and leading/trailing whitespace
        lines = [line.strip("/ ").strip() for line in text.split("\n")]

        for line in lines:
            if not line:
                continue
            if "`SIGNATURE`" in line:
                signature = line.replace("`SIGNATURE`", "").strip()
            elif "`PARAMETERS`" in line:
                raw_params = line.replace("`PARAMETERS`", "").strip().split(',')
                for item in raw_params:
                    match = re.match(r'(.+?)\s*\((.+)\)', item.strip())
                    if match:
                        parameters.append((match.group(1).strip(), match.group(2).strip()))
            elif "`RETURN`" in line:
                parts = line.replace("`RETURN`", "").strip().split(',')
                ret_desc = parts[0].strip() if len(parts) > 0 else ""
                ret_units = parts[1].strip() if len(parts) > 1 else ""
                return_info = (ret_desc, ret_units)
            elif "`DESCRIPTION`" in line:
                description = line.replace("`DESCRIPTION`", "").strip()

        return cls(
            signature=signature,
            parameters=parameters,
            return_info=return_info,
            description=description
        )

    @classmethod
    def from_text_block(cls, text_block: str) -> List['Documentation']:
        """Scrapes all documented functions from a block of CoNES source code."""
        return [cls.from_text(match.group('comments')) for match in FUNCTION_PATTERN.finditer(text_block)]

    @classmethod
    def from_file(cls, path: str) -> List['Documentation']:
        """Scrapes documented functions from a .cnes file."""
        try:
            with open(path, 'r', encoding='utf-8') as f:
                return cls.from_text_block(f.read())
        except Exception:
            return []