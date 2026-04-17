const vscode = require('vscode');

// Updated to include math functions and metadata
const CONSTANTS_STR = "CONST_E::Euler's number, also referred to as Napier's constant|CONST_GRAV:m/s^2:Acceleration due to gravity as measured on earth's surface|CONST_PI::Dimensionless ratio of a circle's circumference to its diameter|CONST_R:J/mol*K:Universal gas constant|CONST_SIGMA:W/m^2*K^4:Stefan-Boltzmann Constant|STD_PRESS_BAR:Bar:Pressure in Pascals at ST&P|STD_PRESS_KPA:kPa:Pressure in Kilopascals at ST&P|STD_PRESS_MPA:MPa:Pressure in Megapascals at ST&P|STD_PRESS_PA:Pa:Pressure in Pascals at ST&P|STD_TEMP_C:C:Temperature in Celsius at ST&P|STD_TEMP_K:K:Temperature in Kelvin at ST&P";
const FUNCTIONS_STR = "Enthalpy:Material, Two independent properties:Yields the specific enthalpy in J/kg of the substance at the provided state.|Pressure:Material, Two independent properties:Yields the pressure in Pascals of the substance at the provided state.|Temperature:Material, Two independent properties:Yields the temperature in Kelvin of the substance at the provided state.|cos:x:Trigonometric cosine (argument in radians).|exp:x:Exponential function (e^x).|log:x:Natural logarithm (base-e).|sin:x:Trigonometric sine (argument in radians).|sqrt:x:Square root of a non-negative number.|tan:x:Trigonometric tangent (argument in radians).";
const SUBSTANCES_STR = "Air|Water";

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

/**
 * Scans the current document for user-defined variables (lhs of := or =).
 */
function getDocumentVariables(document) {
    const text = document.getText();
    const variables = new Set();
    // Match identifiers on the left side of = or := 
    // e.g. P_A = ..., T_amb := ...
    const regex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\s*(?:=|:=)/g;
    let match;
    while ((match = regex.exec(text)) !== null) {
        variables.add(match[1]);
    }
    return Array.from(variables);
}

function activate(context) {
    const CONSTANTS = parseConstants(CONSTANTS_STR);
    const FUNCTIONS = parseFunctions(FUNCTIONS_STR);
    const SUBSTANCES = parseSubstances(SUBSTANCES_STR);

    const provider = vscode.languages.registerCompletionItemProvider(
        { language: 'cnes' },
        {
            provideCompletionItems(document, position) {
                const line = document.lineAt(position).text;
                if (line.trim().startsWith("//")) return;

                const items = [];

                // 1. User Variables (Dynamic Discovery)
                const docVars = getDocumentVariables(document);
                docVars.forEach(v => {
                    const item = new vscode.CompletionItem(v, vscode.CompletionItemKind.Variable);
                    item.detail = "User Variable";
                    items.push(item);
                });

                // 2. Constants
                CONSTANTS.forEach(c => {
                    const item = new vscode.CompletionItem(c.name, vscode.CompletionItemKind.Constant);
                    item.detail = c.unit ? `[${c.unit}] ${c.desc}` : c.desc;
                    item.documentation = new vscode.MarkdownString(`**${c.name}**\n\n${c.desc}`);
                    items.push(item);
                });

                // 3. Substances
                SUBSTANCES.forEach(s => {
                    const item = new vscode.CompletionItem(s, vscode.CompletionItemKind.Keyword);
                    item.detail = "Substance / Material";
                    item.documentation = new vscode.MarkdownString(`**${s}**\n\nThermophysical property provider.`);
                    items.push(item);
                });

                // 4. Functions (Math + Property)
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
}

function deactivate() { }

module.exports = {
    activate,
    deactivate
};
