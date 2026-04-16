const vscode = require('vscode');

const CONSTANTS_STR = "CONST_E::Euler's number, also referred to as Napier's constant|CONST_GRAV:m/s^2:Acceleration due to gravity as measured on earth's surface|CONST_PI::Dimensionless ratio of a circle's circumference to its diameter|CONST_R:J/mol*K:Universal gas constant|CONST_SIGMA:W/m^2*K^4:Stefan-Boltzmann Constant|STD_PRESS_BAR:Bar:Pressure in Pascals at ST&P|STD_PRESS_KPA:kPa:Pressure in Kilopascals at ST&P|STD_PRESS_MPA:MPa:Pressure in Megapascals at ST&P|STD_PRESS_PA:Pa:Pressure in Pascals at ST&P|STD_TEMP_C:C:Temperature in Celsius at ST&P|STD_TEMP_K:K:Temperature in Kelvin at ST&P";

function parseConstants(str) {
    return str.split('|').map(entry => {
        const parts = entry.split(':');

        const name = parts[0] || "";
        const unit = parts[1] || "";
        const desc = parts.slice(2).join(':') || "";

        return { name, unit, desc };
    });
}

function activate(context) {

    const CONSTANTS = parseConstants(CONSTANTS_STR);

    const provider = vscode.languages.registerCompletionItemProvider(
        { language: 'cnes' },
        {
            provideCompletionItems(document, position) {

                const line = document.lineAt(position).text;

                // Optional: skip comments
                if (line.trim().startsWith("//")) return;

                return new vscode.CompletionList(
                    CONSTANTS.map(c => {
                        const item = new vscode.CompletionItem(
                            c.name,
                            vscode.CompletionItemKind.Constant
                        );

                        item.insertText = c.name;
                        item.detail = c.unit ? `${c.unit} — ${c.desc}` : c.desc;

                        const md = new vscode.MarkdownString();
                        md.appendMarkdown(`**${c.name}**\n\n`);

                        if (c.unit) md.appendMarkdown(`**Unit:** \`${c.unit}\`\n\n`);
                        if (c.desc) md.appendMarkdown(c.desc);

                        item.documentation = md;

                        return item;
                    }),
                    false
                );
            }
        },
        ...['.', '_'] // trigger characters
    );

    context.subscriptions.push(provider);
}

function deactivate() { }

module.exports = {
    activate,
    deactivate
};