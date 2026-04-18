const vscode = require('vscode');
const fs = require('fs');
const path = require('path');

// Updated to include math functions and metadata
const CONSTANTS_STR = "CONST_E::Euler's number, also referred to as Napier's constant|CONST_GRAV:m/s^2:Acceleration due to gravity as measured on earth's surface|CONST_PI::Dimensionless ratio of a circle's circumference to its diameter|CONST_R:J/mol*K:Universal gas constant|CONST_SIGMA:W/m^2*K^4:Stefan-Boltzmann Constant|STD_PRESS_BAR:Bar:Pressure in Pascals at ST&P|STD_PRESS_KPA:kPa:Pressure in Kilopascals at ST&P|STD_PRESS_MPA:MPa:Pressure in Megapascals at ST&P|STD_PRESS_PA:Pa:Pressure in Pascals at ST&P|STD_TEMP_C:C:Temperature in Celsius at ST&P|STD_TEMP_K:K:Temperature in Kelvin at ST&P";
const FUNCTIONS_STR = "Conductivity:Material, Two independent properties:Yields the thermal conductivity in W/m*K.|Density:Material, Two independent properties:Yields the density in kg/m^3.|Enthalpy:Material, Two independent properties:Yields the specific enthalpy in J/kg.|Entropy:Material, Two independent properties:Yields the specific entropy in J/kg*K.|InternalEnergy:Material, Two independent properties:Yields the specific internal energy in J/kg.|Prandtl:Material, Two independent properties:Yields the Prandtl number.|Pressure:Material, Two independent properties:Yields the pressure in Pascals.|Q_cond:k, A, dT, L:Fourier's Law (simplified): Q = k*A*dT/L|Q_conv:h, A, Ts, Tinf:Newton's Law of Cooling: Q = h*A*(Ts - Tinf)|Q_rad:eps, A, Ts, Tsur:Stefan-Boltzmann Law: Q = eps*sigma*A*(Ts^4 - Tsur^4)|Quality:Material, Two independent properties:Yields the vapor quality.|SpecificVolume:Material, Two independent properties:Yields the specific volume in m^3/kg.|Temperature:Material, Two independent properties:Yields the temperature in Kelvin.|Viscosity:Material, Two independent properties:Yields the dynamic viscosity in Pa*s.|abs:x:Absolute value|acos:x:Inverse cosine|asin:x:Inverse sine|atan:x:Inverse tangent|ceil:x:Ceiling function|cos:x:Trigonometric cosine (radians)|cosh:x:Hyperbolic cosine|exp:x:Exponential function (e^x)|floor:x:Floor function|log:x:Natural logarithm (base-e)|log10:x:Common logarithm (base-10)|round:x:Round to nearest integer|sin:x:Trigonometric sine (radians)|sinh:x:Hyperbolic sine|sqrt:x:Square root|tan:x:Trigonometric tangent (radians)|tanh:x:Hyperbolic tangent";
const SUBSTANCES_STR = "Air|Argon|CO2|Nitrogen|O2|R12|R134a|Water";

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
 * Scans text for user-defined variables and include directives, ignoring comments.
 */
function scanText(text, variables, processedFiles, currentFilePath) {
    // Strip comments for variable scanning
    // 1. Block comments
    const noBlockComments = text.replace(/\/\*[\s\S]*?\*\//g, (match) => ' '.repeat(match.length));
    // 2. Line comments
    const cleanText = noBlockComments.replace(/\/\/.*/g, (match) => ' '.repeat(match.length));

    // 1. Match identifiers on the left side of = or := 
    const varRegex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\s*(?:=|:=)/g;
    let match;
    while ((match = varRegex.exec(cleanText)) !== null) {
        variables.add(match[1]);
    }

    // 2. Match include directives (scanning the original text for includes is usually safer, 
    // but we'll use cleanText to avoid included-then-commented files)
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
            } catch (err) {
                // Silently ignore file read errors for autocomplete
            }
        }
    }
}

function activate(context) {
    const CONSTANTS = parseConstants(CONSTANTS_STR);
    const FUNCTIONS = parseFunctions(FUNCTIONS_STR);
    const SUBSTANCES = parseSubstances(SUBSTANCES_STR);

    const provider = vscode.languages.registerCompletionItemProvider(
        { language: 'cnes' },
        {
            provideCompletionItems(document, position) {
                // 1. Quick check for line comment
                const line = document.lineAt(position).text;
                const lineBefore = line.substring(0, position.character);
                if (lineBefore.includes("//")) return undefined;

                // 2. Full check for block comment
                const fullText = document.getText();
                const offset = document.offsetAt(position);
                
                // Simple block comment check: find if the current offset is within a /* */ range
                const blockCommentRegex = /\/\*[\s\S]*?\*\//g;
                let blockMatch;
                while ((blockMatch = blockCommentRegex.exec(fullText)) !== null) {
                    if (offset >= blockMatch.index && offset <= (blockMatch.index + blockMatch[0].length)) {
                        return undefined;
                    }
                }

                const items = [];
                const variables = new Set();
                const processedFiles = new Set();
                
                const currentFilePath = document.uri.fsPath;
                processedFiles.add(currentFilePath);
                
                // Scan current document and recursively scan includes
                scanText(fullText, variables, processedFiles, currentFilePath);

                // 1. User Variables (Dynamic Discovery)
                variables.forEach(v => {
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
