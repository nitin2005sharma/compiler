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
    graphPanel: document.querySelector("#graphPanel"),
    graphMeta: document.querySelector("#graphMeta"),
    graphContent: document.querySelector("#graphContent"),
    tabContent: document.querySelector("#tabContent"),
    fitGraphButton: document.querySelector("#fitGraphButton"),
    actualSizeButton: document.querySelector("#actualSizeButton"),
    zoomOutButton: document.querySelector("#zoomOutButton"),
    zoomInButton: document.querySelector("#zoomInButton"),
};

const tabs = [
    { key: "stderr", label: "Diagnostics" },
    { key: "dashboard", label: "Optimization Dashboard" },
    { key: "summary", label: "summary.txt" },
    { key: "optReport", label: "Optimization Explanations" },
    { key: "finalOptimizedCode", label: "Final Optimized Code" },
    { key: "irDiff", label: "IR Diff" },
    { key: "astSvg", label: "AST Graph" },
    { key: "annotatedAstSvg", label: "Annotated AST Graph" },
    { key: "cfgBeforeOptSvg", label: "CFG Before Optimization" },
    { key: "cfgSvg", label: "Optimized CFG" },
    { key: "cfgAnalysisSvg", label: "CFG/Dominator View" },
    { key: "cfgAnalysis", label: "CFG Metadata" },
    { key: "preOptIr", label: "IR Before Optimization" },
    { key: "ir", label: "Optimized IR" },
    { key: "ssaIr", label: "SSA IR" },
    { key: "symbols", label: "Symbol Table" },
    { key: "liveness", label: "Liveness" },
    { key: "astDot", label: "AST DOT" },
    { key: "annotatedAstDot", label: "Annotated AST DOT" },
    { key: "cfgBeforeOptDot", label: "CFG DOT Before Optimization" },
    { key: "cfgDot", label: "Optimized CFG DOT" },
    { key: "cfgAnalysisDot", label: "CFG Analysis DOT" },
    { key: "plainOutput", label: "Plain Output" },
];

let currentResult = null;
let activeTab = "stderr";
let graphZoom = 1;

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

    if (key === "astSvg" || key === "annotatedAstSvg" || key === "cfgBeforeOptSvg" || key === "cfgSvg" || key === "cfgAnalysisSvg") {
        return "";
    }

    if (key === "stderr") {
        return currentResult.stderr || (currentResult.ok ? "No diagnostics. Compilation succeeded." : "Compilation failed without stderr output.");
    }

    if (key === "plainOutput") {
        return currentResult.trace.plainOutput || "No non-trace stdout output.";
    }

    if (key === "finalOptimizedCode") {
        return currentResult.trace.plainOutput ||
            currentResult.files.ir ||
            "No optimized output code was produced.";
    }

    return currentResult.files[key] || `No ${key} output was produced.`;
}

function isGraphTab(key) {
    return key === "astSvg" || key === "annotatedAstSvg" || key === "cfgBeforeOptSvg" || key === "cfgSvg" || key === "cfgAnalysisSvg";
}

function graphLabel(key) {
    return tabs.find((tab) => tab.key === key)?.label || "Graph";
}

function graphState(key) {
    if (!currentResult) {
        return {
            available: false,
            markup: '<p class="graph-empty">Run the compiler to see a graph.</p>',
        };
    }

    const svg = currentResult.visuals?.[key];
    if (svg) {
        return {
            available: true,
            markup: `<div class="graph-stage"><div class="graph-canvas">${svg}</div></div>`,
        };
    }

    if (!currentResult.graphvizPath) {
        return {
            available: false,
            markup: '<p class="graph-empty">Graphviz was not detected by the UI server. Install it or set <code>DOT_PATH</code> so AST/CFG graphs can render visually.</p>',
        };
    }

    return {
        available: false,
        markup: '<p class="graph-empty">No graph output is available for this run.</p>',
    };
}

function parseSummaryMetrics(summary) {
    const metrics = {
        preIr: 0,
        optIr: 0,
        preBlocks: 0,
        optBlocks: 0,
        constantFolds: 0,
        constantPropagations: 0,
        cse: 0,
        deadCode: 0,
        deadStores: 0,
        unreachable: 0,
        loopSimplifications: 0,
        licm: 0,
        preheaders: 0,
        peels: 0,
        unrolls: 0,
        unswitches: 0,
        strength: 0,
        induction: 0,
    };

    const preIr = summary.match(/PRE-OPT IR: Created (\d+) unoptimized IR quads/);
    const optIr = summary.match(/IR: Created (\d+) optimized IR quads/);
    const preCfg = summary.match(/PRE-OPT CFG: Created .* with (\d+) basic blocks, (\d+) control-flow edges/);
    const optCfg = summary.match(/CFG: Created an optimized control-flow graph with (\d+) basic blocks, (\d+) control-flow edges/);
    const opt = summary.match(/OPT: Applied (\d+) constant folds, (\d+) constant propagations, (\d+) common-subexpression eliminations, (\d+) dead-code removals, (\d+) dead-store removals, (\d+) unreachable-code removals, (\d+) loop simplifications, (\d+) loop-invariant hoists, (\d+) loop preheaders, (\d+) loop peels, (\d+) loop unrolls, and (\d+) loop unswitches, (\d+) strength reductions, and (\d+) induction variables optimized/);

    if (preIr) metrics.preIr = Number(preIr[1]);
    if (optIr) metrics.optIr = Number(optIr[1]);
    if (preCfg) metrics.preBlocks = Number(preCfg[1]);
    if (optCfg) metrics.optBlocks = Number(optCfg[1]);
    if (opt) {
        metrics.constantFolds = Number(opt[1]);
        metrics.constantPropagations = Number(opt[2]);
        metrics.cse = Number(opt[3]);
        metrics.deadCode = Number(opt[4]);
        metrics.deadStores = Number(opt[5]);
        metrics.unreachable = Number(opt[6]);
        metrics.loopSimplifications = Number(opt[7]);
        metrics.licm = Number(opt[8]);
        metrics.preheaders = Number(opt[9]);
        metrics.peels = Number(opt[10]);
        metrics.unrolls = Number(opt[11]);
        metrics.unswitches = Number(opt[12]);
        metrics.strength = Number(opt[13]);
        metrics.induction = Number(opt[14]);
    }
    return metrics;
}

function percentChange(before, after) {
    if (!before) {
        return "0%";
    }
    return `${(((before - after) * 100) / before).toFixed(2)}%`;
}

function renderDashboardHtml() {
    const summary = currentResult?.files?.summary || "";
    if (!summary) {
        return '<p class="empty-note">Run the compiler to see optimization metrics.</p>';
    }

    const metrics = parseSummaryMetrics(summary);
    const cards = [
        ["IR reduction", `${metrics.preIr} -> ${metrics.optIr}`, percentChange(metrics.preIr, metrics.optIr)],
        ["CFG blocks", `${metrics.preBlocks} -> ${metrics.optBlocks}`, percentChange(metrics.preBlocks, metrics.optBlocks)],
        ["Constant folds", metrics.constantFolds, "literal and algebraic simplifications"],
        ["Dead removals", metrics.deadCode + metrics.deadStores, "dead code plus dead stores"],
        ["Loop transforms", metrics.licm + metrics.preheaders + metrics.peels + metrics.unrolls + metrics.unswitches + metrics.strength, "LICM, peel, unroll, unswitch, strength"],
        ["Induction vars", metrics.induction, "recognized loop counters"],
    ];

    const cardHtml = cards
        .map(([label, value, hint]) => `
            <article class="metric-card">
                <span>${escapeHtml(String(label))}</span>
                <strong>${escapeHtml(String(value))}</strong>
                <small>${escapeHtml(String(hint))}</small>
            </article>
        `)
        .join("");

    return `
        <section class="dashboard-grid">${cardHtml}</section>
        <section class="pass-list">
            <div><span>Constant propagation</span><strong>${metrics.constantPropagations}</strong></div>
            <div><span>CSE</span><strong>${metrics.cse}</strong></div>
            <div><span>Unreachable removed</span><strong>${metrics.unreachable}</strong></div>
            <div><span>Loop preheaders</span><strong>${metrics.preheaders}</strong></div>
            <div><span>Loop peels</span><strong>${metrics.peels}</strong></div>
            <div><span>Loop unrolls</span><strong>${metrics.unrolls}</strong></div>
            <div><span>Loop unswitches</span><strong>${metrics.unswitches}</strong></div>
            <div><span>Strength reductions</span><strong>${metrics.strength}</strong></div>
        </section>
    `;
}

function buildLineDiff(beforeText, afterText) {
    const before = beforeText.trim().split(/\r?\n/).filter(Boolean);
    const after = afterText.trim().split(/\r?\n/).filter(Boolean);
    const dp = Array.from({ length: before.length + 1 }, () => Array(after.length + 1).fill(0));

    for (let i = before.length - 1; i >= 0; i -= 1) {
        for (let j = after.length - 1; j >= 0; j -= 1) {
            dp[i][j] = before[i] === after[j]
                ? dp[i + 1][j + 1] + 1
                : Math.max(dp[i + 1][j], dp[i][j + 1]);
        }
    }

    const rows = [];
    let i = 0;
    let j = 0;
    while (i < before.length || j < after.length) {
        if (i < before.length && j < after.length && before[i] === after[j]) {
            rows.push({ type: "same", before: before[i], after: after[j] });
            i += 1;
            j += 1;
        } else if (j < after.length && (i === before.length || dp[i][j + 1] >= dp[i + 1][j])) {
            rows.push({ type: "added", before: "", after: after[j] });
            j += 1;
        } else {
            rows.push({ type: "removed", before: before[i], after: "" });
            i += 1;
        }
    }
    return rows;
}

function renderIrDiffHtml() {
    const pre = currentResult?.files?.preOptIr || "";
    const post = currentResult?.files?.ir || "";
    if (!pre || !post) {
        return '<p class="empty-note">Run the compiler to compare IR before and after optimization.</p>';
    }

    const rows = buildLineDiff(pre, post)
        .map((row) => `
            <tr class="${row.type}">
                <td>${escapeHtml(row.before)}</td>
                <td>${escapeHtml(row.after)}</td>
            </tr>
        `)
        .join("");

    return `
        <table class="ir-diff">
            <thead><tr><th>Before Optimization</th><th>After Optimization</th></tr></thead>
            <tbody>${rows}</tbody>
        </table>
    `;
}

function updateGraphMeta(key, available) {
    if (!elements.graphMeta) {
        return;
    }

    if (!available) {
        elements.graphMeta.textContent = `${graphLabel(key)} unavailable for this run.`;
        return;
    }

    elements.graphMeta.textContent = `${graphLabel(key)} at ${Math.round(graphZoom * 100)}%`;
}

function applyGraphZoom() {
    const canvas = elements.graphContent?.querySelector(".graph-canvas");
    if (!canvas) {
        updateGraphMeta(activeTab, false);
        return;
    }

    canvas.style.transform = `scale(${graphZoom})`;
    updateGraphMeta(activeTab, true);
}

function setGraphZoom(nextZoom) {
    graphZoom = Math.max(0.2, Math.min(3, nextZoom));
    applyGraphZoom();
}

function fitGraphToViewport() {
    const svg = elements.graphContent?.querySelector("svg");
    if (!svg || !elements.graphContent) {
        return;
    }

    const viewBox = svg.viewBox?.baseVal;
    const width =
        (viewBox && viewBox.width) ||
        Number(svg.getAttribute("width")) ||
        svg.getBoundingClientRect().width ||
        1;
    const height =
        (viewBox && viewBox.height) ||
        Number(svg.getAttribute("height")) ||
        svg.getBoundingClientRect().height ||
        1;
    const availableWidth = Math.max(elements.graphContent.clientWidth - 36, 1);
    const availableHeight = Math.max(elements.graphContent.clientHeight - 36, 1);
    const widthZoom = availableWidth / width;
    const heightZoom = availableHeight / height;

    // Favor width fitting so very tall graphs, especially the annotated AST,
    // remain visible and readable instead of being crushed to a tiny scale.
    const fittedZoom =
        heightZoom < 0.35
            ? Math.min(1, widthZoom)
            : Math.min(1, widthZoom, heightZoom);
    setGraphZoom(fittedZoom);
    elements.graphContent.scrollTop = 0;
    elements.graphContent.scrollLeft = 0;
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
        const state = graphState(activeTab);
        elements.graphPanel?.classList.add("active");
        elements.graphContent.classList.add("active");
        elements.tabContent.style.display = "none";
        elements.graphContent.innerHTML = state.markup;
        updateGraphMeta(activeTab, state.available);
        if (state.available) {
            requestAnimationFrame(() => {
                fitGraphToViewport();
            });
        }
    } else {
        elements.graphPanel?.classList.remove("active");
        elements.graphContent.classList.remove("active");
        elements.graphContent.innerHTML = "";
        elements.tabContent.style.display = "block";
        elements.tabContent.className = "output-view";
        if (activeTab === "dashboard") {
            elements.tabContent.classList.add("rich-output");
            elements.tabContent.innerHTML = renderDashboardHtml();
        } else if (activeTab === "irDiff") {
            elements.tabContent.classList.add("rich-output");
            elements.tabContent.innerHTML = renderIrDiffHtml();
        } else {
            elements.tabContent.textContent = tabValue(activeTab);
        }
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
            files: { summary: "", optReport: "", ssaIr: "", symbols: "", liveness: "", cfgAnalysis: "", preOptIr: "", ir: "", astDot: "", annotatedAstDot: "", cfgBeforeOptDot: "", cfgDot: "", cfgAnalysisDot: "" },
            visuals: { astSvg: "", annotatedAstSvg: "", cfgBeforeOptSvg: "", cfgSvg: "", cfgAnalysisSvg: "" },
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
elements.fitGraphButton?.addEventListener("click", fitGraphToViewport);
elements.actualSizeButton?.addEventListener("click", () => setGraphZoom(1));
elements.zoomOutButton?.addEventListener("click", () => setGraphZoom(graphZoom - 0.1));
elements.zoomInButton?.addEventListener("click", () => setGraphZoom(graphZoom + 0.1));

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
                summary: "summary.txt output will appear here after a run.",
                optReport: "Optimization explanations will appear here after a run.",
                ssaIr: "SSA-style IR output will appear here after a run.",
                symbols: "Symbol table output will appear here after a run.",
                liveness: "Liveness analysis output will appear here after a run.",
                cfgAnalysis: "CFG metadata output will appear here after a run.",
                preOptIr: "Pre-optimization IR output will appear here after a run.",
                ir: "IR output will appear here after a run.",
                astDot: "AST DOT output will appear here after a run.",
                annotatedAstDot: "Annotated AST DOT output will appear here after a run.",
                cfgBeforeOptDot: "Pre-optimization CFG DOT output will appear here after a run.",
                cfgDot: "Optimized CFG DOT output will appear here after a run.",
                cfgAnalysisDot: "CFG analysis DOT output will appear here after a run.",
            },
            visuals: {
                astSvg: "",
                annotatedAstSvg: "",
                cfgBeforeOptSvg: "",
                cfgSvg: "",
                cfgAnalysisSvg: "",
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
            files: { summary: "", optReport: "", ssaIr: "", symbols: "", liveness: "", cfgAnalysis: "", preOptIr: "", ir: "", astDot: "", annotatedAstDot: "", cfgBeforeOptDot: "", cfgDot: "", cfgAnalysisDot: "" },
            visuals: { astSvg: "", annotatedAstSvg: "", cfgBeforeOptSvg: "", cfgSvg: "", cfgAnalysisSvg: "" },
        });
    });
