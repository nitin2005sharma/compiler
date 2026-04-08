const elements = {
    sampleSelect: document.querySelector("#sampleSelect"),
    sourceEditor: document.querySelector("#sourceEditor"),
    traceToggle: document.querySelector("#traceToggle"),
    runButton: document.querySelector("#runButton"),
    compilerPathLabel: document.querySelector("#compilerPathLabel"),
    graphvizPathLabel: document.querySelector("#graphvizPathLabel"),
    statusPill: document.querySelector("#statusPill"),
    summaryCards: document.querySelector("#summaryCards"),
    phaseTrace: document.querySelector("#phaseTrace"),
    tabButtons: document.querySelector("#tabButtons"),
    graphContent: document.querySelector("#graphContent"),
    tabContent: document.querySelector("#tabContent"),
};

const tabs = [
    { key: "stderr", label: "Diagnostics" },
    { key: "astSvg", label: "AST Graph" },
    { key: "annotatedAstSvg", label: "Annotated AST Graph" },
    { key: "cfgSvg", label: "CFG Graph" },
    { key: "ir", label: "IR" },
    { key: "astDot", label: "AST DOT" },
    { key: "annotatedAstDot", label: "Annotated AST DOT" },
    { key: "cfgDot", label: "CFG DOT" },
    { key: "plainOutput", label: "Plain Output" },
];

let currentResult = null;
let activeTab = "stderr";

function escapeHtml(value) {
    return value
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;");
}

function setStatus(mode, label) {
    elements.statusPill.className = `status-pill ${mode}`;
    elements.statusPill.textContent = label;
}

function setElementText(element, value) {
    if (element) {
        element.textContent = value;
    }
}

function makeCard(title, value, hint = "") {
    const card = document.createElement("article");
    card.className = "summary-card";
    card.innerHTML = `<div>${title}</div><strong>${value}</strong>${hint ? `<small>${hint}</small>` : ""}`;
    return card;
}

function setSamplePlaceholder(label, disabled = true) {
    elements.sampleSelect.innerHTML = "";
    const option = document.createElement("option");
    option.value = "";
    option.textContent = label;
    option.selected = true;
    option.disabled = disabled;
    elements.sampleSelect.appendChild(option);
    elements.sampleSelect.disabled = disabled;
}

function renderSummary(result) {
    elements.summaryCards.innerHTML = "";
    const cards = [
        makeCard("Exit Code", String(result.code), result.ok ? "Compilation completed" : "Compilation stopped with an error"),
    ];
    for (const card of cards) {
        elements.summaryCards.appendChild(card);
    }
}

function renderTrace(result) {
    elements.phaseTrace.innerHTML = "";

    if (result.trace.phases.length === 0) {
        const empty = document.createElement("article");
        empty.className = "phase-card";
        empty.innerHTML = "<h4>No trace</h4><p>Enable trace mode to see each phase step-by-step.</p>";
        elements.phaseTrace.appendChild(empty);
        return;
    }

    for (const phase of result.trace.phases) {
        const card = document.createElement("article");
        card.className = "phase-card";
        const items = result.trace.byPhase[phase]
            .map((line) => `<li>${escapeHtml(line)}</li>`)
            .join("");
        card.innerHTML = `<h4>${phase}</h4><ul>${items}</ul>`;
        elements.phaseTrace.appendChild(card);
    }
}

function tabValue(key) {
    if (!currentResult) {
        return "";
    }

    if (key === "astSvg" || key === "annotatedAstSvg" || key === "cfgSvg") {
        return "";
    }

    if (key === "stderr") {
        return currentResult.stderr || (currentResult.ok ? "No diagnostics. Compilation succeeded." : "Compilation failed without stderr output.");
    }

    if (key === "plainOutput") {
        return currentResult.trace.plainOutput || "No non-trace stdout output.";
    }

    return currentResult.files[key] || `No ${key} output was produced.`;
}

function isGraphTab(key) {
    return key === "astSvg" || key === "annotatedAstSvg" || key === "cfgSvg";
}

function graphMarkup(key) {
    if (!currentResult) {
        return '<p class="graph-empty">Run the compiler to see a graph.</p>';
    }

    const svg = currentResult.visuals?.[key];
    if (svg) {
        return svg;
    }

    if (!currentResult.graphvizPath) {
        return '<p class="graph-empty">Graphviz was not detected by the UI server. Install it or set <code>DOT_PATH</code> so AST/CFG graphs can render visually.</p>';
    }

    return '<p class="graph-empty">No graph output is available for this run.</p>';
}

function renderTabs() {
    elements.tabButtons.innerHTML = "";
    for (const tab of tabs) {
        const button = document.createElement("button");
        button.type = "button";
        button.textContent = tab.label;
        if (tab.key === activeTab) {
            button.classList.add("active");
        }
        button.addEventListener("click", () => {
            activeTab = tab.key;
            renderTabs();
        });
        elements.tabButtons.appendChild(button);
    }

    if (isGraphTab(activeTab)) {
        elements.graphContent.classList.add("active");
        elements.tabContent.style.display = "none";
        elements.graphContent.innerHTML = graphMarkup(activeTab);
    } else {
        elements.graphContent.classList.remove("active");
        elements.graphContent.innerHTML = "";
        elements.tabContent.style.display = "block";
        elements.tabContent.textContent = tabValue(activeTab);
    }
}

function renderResult(result) {
    currentResult = result;
    setStatus(result.ok ? "success" : "error", result.ok ? "Compiled" : "Error");
    renderSummary(result);
    renderTrace(result);
    renderTabs();
}

async function fetchSamples() {
    setSamplePlaceholder("Loading samples...");

    const response = await fetch("/api/samples");
    const contentType = response.headers.get("content-type") || "";
    const payload = contentType.includes("application/json")
        ? await response.json()
        : await response.text();

    if (!response.ok) {
        const message =
            typeof payload === "object" && payload?.error
                ? payload.error
                : `Could not load samples from /api/samples (${response.status} ${response.statusText}). Start the UI with "node .\\ui\\server.mjs" and open http://127.0.0.1:4318.`;
        throw new Error(message);
    }

    setElementText(elements.compilerPathLabel, payload.compilerPath || "Build not found");
    setElementText(elements.graphvizPathLabel, payload.graphvizPath || "Not detected");

    if (!Array.isArray(payload.samples) || payload.samples.length === 0) {
        setSamplePlaceholder("No samples found in examples/");
        elements.sourceEditor.value = "";
        return;
    }

    elements.sampleSelect.innerHTML = "";
    elements.sampleSelect.disabled = false;
    for (const sample of payload.samples) {
        const option = document.createElement("option");
        option.value = sample.name;
        option.textContent = sample.name;
        option.dataset.source = sample.source;
        elements.sampleSelect.appendChild(option);
    }

    const first = elements.sampleSelect.options[0];
    if (first) {
        elements.sourceEditor.value = first.dataset.source;
    }
}

async function runCompiler() {
    elements.runButton.disabled = true;
    setStatus("running", "Compiling...");

    try {
        const response = await fetch("/api/compile", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                source: elements.sourceEditor.value,
                trace: elements.traceToggle.checked,
            }),
        });

        const payload = await response.json();
        if (!response.ok) {
            throw new Error(payload.error || "Compilation request failed.");
        }

        renderResult(payload);
    } catch (error) {
        renderResult({
            ok: false,
            code: 1,
            compilerPath: "",
            graphvizPath: "",
            stderr: error instanceof Error ? error.message : "Unexpected UI error",
            trace: { phases: [], byPhase: {}, plainOutput: "" },
            files: { ir: "", astDot: "", annotatedAstDot: "", cfgDot: "" },
            visuals: { astSvg: "", annotatedAstSvg: "", cfgSvg: "" },
        });
    } finally {
        elements.runButton.disabled = false;
    }
}

elements.sampleSelect.addEventListener("change", () => {
    const selected = elements.sampleSelect.selectedOptions[0];
    if (selected) {
        elements.sourceEditor.value = selected.dataset.source;
    }
});

elements.runButton.addEventListener("click", runCompiler);

fetchSamples()
    .then(() => {
        renderResult({
            ok: true,
            code: 0,
            compilerPath: "",
            graphvizPath: "",
            stderr: "Run the compiler to see diagnostics here.",
            trace: { phases: [], byPhase: {}, plainOutput: "" },
            files: {
                ir: "IR output will appear here after a run.",
                astDot: "AST DOT output will appear here after a run.",
                annotatedAstDot: "Annotated AST DOT output will appear here after a run.",
                cfgDot: "CFG DOT output will appear here after a run.",
            },
            visuals: {
                astSvg: "",
                annotatedAstSvg: "",
                cfgSvg: "",
            },
        });
    })
    .catch((error) => {
        setSamplePlaceholder("Samples unavailable");
        renderResult({
            ok: false,
            code: 1,
            compilerPath: "",
            graphvizPath: "",
            stderr: error instanceof Error ? error.message : "Failed to load UI data",
            trace: { phases: [], byPhase: {}, plainOutput: "" },
            files: { ir: "", astDot: "", annotatedAstDot: "", cfgDot: "" },
            visuals: { astSvg: "", annotatedAstSvg: "", cfgSvg: "" },
        });
    });
