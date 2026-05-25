import { useEffect, useRef } from "react";
import Editor, { type BeforeMount, type OnMount } from "@monaco-editor/react";
import type * as Monaco from "monaco-editor";
import { registerCnesLanguage } from "@/features/workbench/monaco-cnes-language";
import type { LintDiagnostic } from "@/features/workbench/workbench-lint-diagnostics";

type MonacoSourceEditorProps = {
  value: string;
  onChange: (value: string) => void;
  diagnostics?: LintDiagnostic[];
  readOnly?: boolean;
};

export function MonacoSourceEditor({ value, onChange, diagnostics = [], readOnly = false }: MonacoSourceEditorProps) {
  const monacoRef = useRef<typeof Monaco | null>(null);
  const editorRef = useRef<Monaco.editor.IStandaloneCodeEditor | null>(null);

  const handleBeforeMount: BeforeMount = (monaco) => {
    monacoRef.current = monaco as typeof Monaco;
    registerCnesLanguage(monaco as typeof Monaco);
  };

  const handleMount: OnMount = (editor, monaco) => {
    editorRef.current = editor;
    monacoRef.current = monaco as typeof Monaco;
    editor.focus();
  };

  useEffect(() => {
    const monaco = monacoRef.current;
    const editor = editorRef.current;
    const model = editor?.getModel();
    if (!monaco || !model) {
      return;
    }

    const markers = diagnostics
      .filter((diagnostic) => diagnostic.line && diagnostic.line <= model.getLineCount())
      .map((diagnostic) => {
        const line = diagnostic.line || 1;
        return {
          severity: markerSeverity(monaco, diagnostic.severity),
          message: diagnostic.message,
          source: diagnostic.source,
          startLineNumber: line,
          startColumn: 1,
          endLineNumber: line,
          endColumn: model.getLineMaxColumn(line),
        };
      });

    monaco.editor.setModelMarkers(model, "cnes-lint", markers);

    return () => {
      monaco.editor.setModelMarkers(model, "cnes-lint", []);
    };
  }, [diagnostics, value]);

  return (
    <div className="relative min-h-[520px] flex-1" data-workbench-monaco-editor>
      <Editor
        beforeMount={handleBeforeMount}
        defaultLanguage="cnes"
        language="cnes"
        theme="vs-dark"
        value={value}
        onChange={(nextValue) => onChange(nextValue ?? "")}
        onMount={handleMount}
        options={{
          automaticLayout: true,
          bracketPairColorization: { enabled: true },
          fontFamily: "ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace",
          fontSize: 13,
          lineHeight: 22,
          lineNumbers: "on",
          minimap: { enabled: false },
          padding: { top: 14, bottom: 14 },
          readOnly,
          scrollBeyondLastLine: false,
          tabSize: 2,
          wordWrap: "on",
        }}
      />
      <textarea
        aria-hidden="true"
        className="sr-only"
        data-workbench-source-test-input
        tabIndex={-1}
        value={value}
        onChange={(event) => onChange(event.target.value)}
      />
    </div>
  );
}

function markerSeverity(monaco: typeof Monaco, severity: LintDiagnostic["severity"]) {
  if (severity === "warning") {
    return monaco.MarkerSeverity.Warning;
  }

  if (severity === "info") {
    return monaco.MarkerSeverity.Info;
  }

  return monaco.MarkerSeverity.Error;
}
