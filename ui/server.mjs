import { spawn } from "node:child_process";
import { promises as fs } from "node:fs";
import http from "node:http";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoDir = path.resolve(__dirname, "..");
const publicDir = path.join(__dirname, "public");
const examplesDir = path.join(repoDir, "examples");
const defaultPort = Number(process.env.PORT || 4318);

async function findDotExecutable() {
    if (process.env.DOT_PATH && await fileExists(process.env.DOT_PATH)) {
        return process.env.DOT_PATH;
    }

    const commonPaths = [
        "C:\\Program Files\\Graphviz\\bin\\dot.exe",
        "C:\\Program Files (x86)\\Graphviz\\bin\\dot.exe",
    ];

    for (const candidate of commonPaths) {
        if (await fileExists(candidate)) {
            return candidate;
        }
    }

    return "";
}

function json(res, status, payload) {
    const body = JSON.stringify(payload);
    res.writeHead(status, {
        "Content-Type": "application/json; charset=utf-8",
        "Content-Length": Buffer.byteLength(body),
        "Cache-Control": "no-store",
    });
    res.end(body);
}

function text(res, status, payload, contentType = "text/plain; charset=utf-8") {
    res.writeHead(status, {
        "Content-Type": contentType,
        "Content-Length": Buffer.byteLength(payload),
        "Cache-Control": "no-store",
    });
    res.end(payload);
}

async function maybeRead(filePath) {
    try {
        return await fs.readFile(filePath, "utf8");
    } catch {
        return "";
    }
}

async function fileExists(filePath) {
    try {
        await fs.access(filePath);
        return true;
    } catch {
        return false;
    }
}

async function findCompilerExecutable() {
    if (process.env.COMPILER_PATH) {
        return process.env.COMPILER_PATH;
    }

    const candidates = [];
    const entries = await fs.readdir(repoDir, { withFileTypes: true });
    for (const entry of entries) {
        if (!entry.isDirectory() || !entry.name.startsWith("build")) {
            continue;
        }

        const buildDir = path.join(repoDir, entry.name);
        for (const relativePath of ["Debug/compiler.exe", "Release/compiler.exe", "compiler"]) 
            {
            const compilerPath = path.join(buildDir, relativePath);
            if (await fileExists(compilerPath)) {
                const stat = await fs.stat(compilerPath);
                candidates.push({ compilerPath, mtimeMs: stat.mtimeMs });
            }
        }
    }

    candidates.sort((left, right) => right.mtimeMs - left.mtimeMs);
    return candidates[0]?.compilerPath ?? "";
}

function parseTraceOutput(stdout) {
    const phases = [];
    const byPhase = {};
    const plainLines = [];

    for (const line of stdout.split(/\r?\n/)) {
        if (!line.trim()) {
            continue;
        }

        const match = line.match(/^\[TRACE\]\[([A-Z]+)\]\s?(.*)$/);
        if (match) {
            const phase = match[1];
            const message = match[2];
            if (!byPhase[phase]) {
                byPhase[phase] = [];
                phases.push(phase);
            }
            byPhase[phase].push(message);
        } else {
            plainLines.push(line);
        }
    }

    return {
        phases,
        byPhase,
        plainOutput: plainLines.join("\n"),
    };
}

function runCompiler(compilerPath, args, cwd) {
    return new Promise((resolve, reject) => {
        const child = spawn(compilerPath, args, {
            cwd,
            windowsHide: true,
        });

        let stdout = "";
        let stderr = "";

        child.stdout.on("data", (chunk) => {
            stdout += chunk.toString();
        });

        child.stderr.on("data", (chunk) => {
            stderr += chunk.toString();
        });

        child.on("error", reject);
        child.on("close", (code) => {
            resolve({ code: code ?? 1, stdout, stderr });
        });
    });
}

async function compileSource(source, traceEnabled = true) {
    const compilerPath = await findCompilerExecutable();
    const graphvizPath = await findDotExecutable();
    if (!compilerPath) {
        return {
            ok: false,
            code: 1,
            stdout: "",
            stderr: "Compiler executable not found. Build the C++ project first.",
            compilerPath: "",
            graphvizPath,
            files: { ir: "", astDot: "", annotatedAstDot: "", cfgDot: "" },
            visuals: { astSvg: "", annotatedAstSvg: "", cfgSvg: "" },
            trace: { phases: [], byPhase: {}, plainOutput: "" },
        };
    }

    const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), "compiler-ui-"));
    const sourcePath = path.join(tempDir, "program.src");
    const outputPrefix = path.join(tempDir, "result");

    try {
        await fs.writeFile(sourcePath, source, "utf8");

        const args = [];
        if (traceEnabled) {
            args.push("--trace");
        }
        args.push("--no-png", sourcePath, outputPrefix);

        const result = await runCompiler(compilerPath, args, repoDir);
        const trace = parseTraceOutput(result.stdout);
        const astDotPath = outputPrefix + "_ast.dot";
        const annotatedAstDotPath = outputPrefix + "_annotated_ast.dot";
        const cfgDotPath = outputPrefix + "_cfg.dot";
        const astDot = await maybeRead(astDotPath);
        const annotatedAstDot = await maybeRead(annotatedAstDotPath);
        const cfgDot = await maybeRead(cfgDotPath);

        const visuals = {
            astSvg: "",
            annotatedAstSvg: "",
            cfgSvg: "",
        };

        if (graphvizPath && astDot) {
            const astSvgResult = await runCompiler(graphvizPath, ["-Tsvg", astDotPath], repoDir);
            if (astSvgResult.code === 0) {
                visuals.astSvg = astSvgResult.stdout;
            }
        }

        if (graphvizPath && annotatedAstDot) {
            const annotatedAstSvgResult = await runCompiler(graphvizPath, ["-Tsvg", annotatedAstDotPath], repoDir);
            if (annotatedAstSvgResult.code === 0) {
                visuals.annotatedAstSvg = annotatedAstSvgResult.stdout;
            }
        }

        if (graphvizPath && cfgDot) {
            const cfgSvgResult = await runCompiler(graphvizPath, ["-Tsvg", cfgDotPath], repoDir);
            if (cfgSvgResult.code === 0) {
                visuals.cfgSvg = cfgSvgResult.stdout;
            }
        }

        return {
            ok: result.code === 0,
            code: result.code,
            stdout: result.stdout,
            stderr: result.stderr,
            compilerPath,
            graphvizPath,
            files: {
                ir: await maybeRead(outputPrefix + "_ir.txt"),
                astDot,
                annotatedAstDot,
                cfgDot,
            },
            visuals,
            trace,
        };
    } finally {
        await fs.rm(tempDir, { recursive: true, force: true });
    }
}

async function loadSamples() {
    const entries = await fs.readdir(examplesDir, { withFileTypes: true });
    const sampleFiles = entries
        .filter((entry) => entry.isFile() && entry.name.endsWith(".src"))
        .map((entry) => entry.name)
        .sort((left, right) => left.localeCompare(right));

    const samples = [];
    for (const name of sampleFiles) {
        samples.push({
            name,
            source: await fs.readFile(path.join(examplesDir, name), "utf8"),
        });
    }
    return samples;
}

async function readJsonBody(req) {
    const chunks = [];
    for await (const chunk of req) {
        chunks.push(chunk);
    }

    if (chunks.length === 0) {
        return {};
    }

    return JSON.parse(Buffer.concat(chunks).toString("utf8"));
}

async function serveStatic(req, res) {
    const requestPath = req.url === "/" ? "/index.html" : req.url;
    const safePath = path.normalize(requestPath).replace(/^(\.\.[/\\])+/, "");
    const filePath = path.join(publicDir, safePath);

    if (!filePath.startsWith(publicDir)) {
        text(res, 403, "Forbidden");
        return;
    }

    try {
        const stat = await fs.stat(filePath);
        if (!stat.isFile()) {
            text(res, 404, "Not found");
            return;
        }

        const ext = path.extname(filePath).toLowerCase();
        const contentType =
            ext === ".html" ? "text/html; charset=utf-8" :
            ext === ".css" ? "text/css; charset=utf-8" :
            ext === ".js" ? "application/javascript; charset=utf-8" :
            "text/plain; charset=utf-8";

        const content = await fs.readFile(filePath);
        res.writeHead(200, {
            "Content-Type": contentType,
            "Content-Length": content.length,
            "Cache-Control": "no-store",
        });
        res.end(content);
    } catch {
        text(res, 404, "Not found");
    }
}

function createServer() {
    return http.createServer(async (req, res) => {
        try {
            if (req.method === "GET" && req.url === "/api/samples") {
                json(res, 200, {
                    samples: await loadSamples(),
                    compilerPath: await findCompilerExecutable(),
                    graphvizPath: await findDotExecutable(),
                });
                return;
            }

            if (req.method === "POST" && req.url === "/api/compile") {
                const body = await readJsonBody(req);
                const source = typeof body.source === "string" ? body.source : "";
                const traceEnabled = body.trace !== false;

                if (!source.trim()) {
                    json(res, 400, { error: "Source code is empty." });
                    return;
                }

                json(res, 200, await compileSource(source, traceEnabled));
                return;
            }

            if (req.method === "GET") {
                await serveStatic(req, res);
                return;
            }

            text(res, 405, "Method not allowed");
        } catch (error) {
            json(res, 500, {
                error: error instanceof Error ? error.message : "Unexpected server error",
            });
        }
    });
}

async function startServer(port = defaultPort) {
    const server = createServer();
    return new Promise((resolve) => {
        server.listen(port, "127.0.0.1", () => {
            console.log(`Compiler UI running at http://127.0.0.1:${port}`);
            resolve(server);
        });
    });
}

const isMainModule =
    process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);

if (isMainModule) {
    startServer();
}

export { compileSource, createServer, findCompilerExecutable, parseTraceOutput, startServer };
