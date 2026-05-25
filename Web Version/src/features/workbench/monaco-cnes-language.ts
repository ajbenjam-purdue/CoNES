import type * as Monaco from "monaco-editor";

const languageId = "cnes";

export function registerCnesLanguage(monaco: typeof Monaco) {
  if (monaco.languages.getLanguages().some((language) => language.id === languageId)) {
    return;
  }

  monaco.languages.register({
    id: languageId,
    extensions: [".cnes"],
    aliases: ["CoNES", "cnes"],
  });

  monaco.languages.setLanguageConfiguration(languageId, {
    comments: {
      lineComment: "//",
    },
    brackets: [
      ["{", "}"],
      ["[", "]"],
      ["(", ")"],
    ],
    autoClosingPairs: [
      { open: "{", close: "}" },
      { open: "[", close: "]" },
      { open: "(", close: ")" },
      { open: '"', close: '"' },
    ],
  });

  monaco.languages.setMonarchTokensProvider(languageId, {
    tokenizer: {
      root: [
        [/\/\/.*$/, "comment"],
        [/"[^"]*"/, "string"],
        [/\b(include|if|else|for|in|return|true|false)\b/, "keyword"],
        [/[a-zA-Z_][\w]*(?=\()/, "function"],
        [/[a-zA-Z_][\w]*(?=\s*:=)/, "variable"],
        [/\b\d+(?:\.\d+)?(?:e[+-]?\d+)?\b/i, "number"],
        [/:=|==|!=|<=|>=|[-+*/=<>]/, "operator"],
        [/[{}()[\]]/, "@brackets"],
      ],
    },
  });
}
