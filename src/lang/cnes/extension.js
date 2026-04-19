const vscode = require('vscode');
const fs = require('fs');
const path = require('path');
const cp = require('child_process');

// Fallback strings if cnes.exe is not found
let CONSTANTS_STR = "CONST_E::Euler's number|CONST_GRAV:m/s^2:Gravity|CONST_PI::Pi|CONST_R:J/mol*K:Gas constant|CONST_SIGMA:W/m^2*K^4:Stefan-Boltzmann|STD_PRESS_PA:Pa:Standard pressure";
let FUNCTIONS_STR = "Enthalpy:Material, Two properties:Specific enthalpy|Entropy:Material, Two properties:Specific entropy|Pressure:Material, Two properties:Pressure|Temperature:Material, Two properties:Temperature|sin:x:Sine|cos:x:Cosine|sqrt:x:Square root";
let SUBSTANCES_STR = "Air|Water|R134a|R12";

function fetchMetadata() {
    try {
        let command = "cnes";
        
        // 1. Try to find cnes in workspace root if not in path
        if (vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0) {
            const root = vscode.workspace.workspaceFolders[0].uri.fsPath;
            const possiblePaths = [
                path.join(root, "cnes.exe"),
                path.join(root, "cnes"),
                path.join(root, "bin", "cnes.exe"),
                path.join(root, "bin", "cnes")
            ];
            
            for (const p of possiblePaths) {
                if (fs.existsSync(p)) {
                    command = `"${p}"`;
                    break;
                }
            }
        }

        const fullCommand = `${command} --out-vscode-metadata`;
        console.log(`CoNES: Executing ${fullCommand}`);
        
        const output = cp.execSync(fullCommand, { encoding: 'utf8', timeout: 3000 });
        
        if (output && output.includes("|||")) {
            const parts = output.split("|||");
            if (parts.length === 3) {
                CONSTANTS_STR = parts[0].trim();
                FUNCTIONS_STR = parts[1].trim();
                SUBSTANCES_STR = parts[2].trim();
                console.log("CoNES: Successfully updated metadata from cnes.exe");
                return "Success";
            }
        }
        return "Invalid Format";
    } catch (err) {
        console.log("CoNES: Could not auto-update metadata. " + err.message);
        return "Error: " + err.message;
    }
}

function parseConstants(str) {
    if (!str) return [];
    return str.split('|').map(entry => {
        const parts = entry.split(':');
        const name = parts[0] || "";
        const unit = parts[1] || "";
        const desc = parts.slice(2).join(':') || "";
        return { name, unit, desc };
    });
}

function parseFunctions(str) {
    if (!str) return [];
    return str.split('|').map(entry => {
        const parts = entry.split(':');
        const name = parts[0] || "";
        const args = parts[1] || "";
        const desc = parts.slice(2).join(':') || "";
        return { name, args, desc };
    });
}

function parseSubstances(str) {
    if (!str) return [];
    return str.split('|');
}

function scanText(text, variables, processedFiles, currentFilePath) {
    const noBlockComments = text.replace(/\/\*[\s\S]*?\*\//g, (match) => ' '.repeat(match.length));
    const cleanText = noBlockComments.replace(/\/\/.*/g, (match) => ' '.repeat(match.length));

    const varRegex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\s*(?:=|:=)/g;
    let match;
    while ((match = varRegex.exec(cleanText)) !== null) {
        variables.add(match[1]);
    }

    const includeRegex = /\binclude\s+"([^"]+)"/g;
    while ((match = includeRegex.exec(cleanText)) !== null) {
        const includePath = match[1];
        let absolutePath = includePath;
        if (currentFilePath && !path.isAbsolute(includePath)) {
            absolutePath = path.resolve(path.dirname(currentFilePath), includePath);
        }

        if (!processedFiles.has(absolutePath)) {
            processedFiles.add(absolutePath);
            try {
                if (fs.existsSync(absolutePath)) {
                    const includedText = fs.readFileSync(absolutePath, 'utf8');
                    scanText(includedText, variables, processedFiles, absolutePath);
                }
            } catch (err) { }
        }
    }
}

function activate(context) {
    fetchMetadata();

    const provider = vscode.languages.registerCompletionItemProvider(
        { language: 'cnes' },
        {
            provideCompletionItems(document, position) {
                const line = document.lineAt(position).text;
                const lineBefore = line.substring(0, position.character);
                if (lineBefore.includes("//")) return undefined;

                const fullText = document.getText();
                const offset = document.offsetAt(position);
                const blockCommentRegex = /\/\*[\s\S]*?\*\//g;
                let blockMatch;
                while ((blockMatch = blockCommentRegex.exec(fullText)) !== null) {
                    if (offset >= blockMatch.index && offset <= (blockMatch.index + blockMatch[0].length)) {
                        return undefined;
                    }
                }

                const CONSTANTS = parseConstants(CONSTANTS_STR);
                const FUNCTIONS = parseFunctions(FUNCTIONS_STR);
                const SUBSTANCES = parseSubstances(SUBSTANCES_STR);

                const items = [];
                const variables = new Set();
                const processedFiles = new Set();
                const currentFilePath = document.uri.fsPath;
                processedFiles.add(currentFilePath);
                
                scanText(fullText, variables, processedFiles, currentFilePath);

                variables.forEach(v => {
                    const item = new vscode.CompletionItem(v, vscode.CompletionItemKind.Variable);
                    item.detail = "User Variable";
                    items.push(item);
                });

                CONSTANTS.forEach(c => {
                    const item = new vscode.CompletionItem(c.name, vscode.CompletionItemKind.Constant);
                    item.detail = c.unit ? `[${c.unit}] ${c.desc}` : c.desc;
                    item.documentation = new vscode.MarkdownString(`**${c.name}**\n\n${c.desc}`);
                    items.push(item);
                });

                SUBSTANCES.forEach(s => {
                    const item = new vscode.CompletionItem(s, vscode.CompletionItemKind.Keyword);
                    item.detail = "Substance / Material";
                    item.documentation = new vscode.MarkdownString(`**${s}**\n\nThermophysical property provider.`);
                    items.push(item);
                });

                FUNCTIONS.forEach(f => {
                    const item = new vscode.CompletionItem(f.name, vscode.CompletionItemKind.Function);
                    item.detail = `${f.name}(${f.args})`;
                    item.insertText = new vscode.SnippetString(`${f.name}($0)`);
                    item.documentation = new vscode.MarkdownString(`**${f.name}**\n\n**Arguments:** ${f.args}\n\n${f.desc}`);
                    items.push(item);
                });

                return items;
            }
        }
    );

    context.subscriptions.push(provider);

    let disposable = vscode.commands.registerCommand('cnes.refreshMetadata', function () {
        const res = fetchMetadata();
        vscode.window.showInformationMessage('CoNES: Metadata refresh result: ' + res);
    });
    context.subscriptions.push(disposable);
}

function deactivate() { }

module.exports = {
    activate,
    deactivate
};
