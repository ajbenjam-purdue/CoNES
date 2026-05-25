import {
  lintArgv,
  runArgv,
  workerPrewarmResultSchema,
  workerResultSchema,
  type WorkerLintResult,
  type WorkerPrewarmResultMessage,
  type WorkerRunResult,
  type WorkerStatusMessage,
} from "@/features/runtime/worker-contract";

const DEFAULT_WORKER_URL = "/runtime/cnes-worker.js";

type WorkerClientOptions = {
  workerUrl?: string;
};

type RunSourceInput = {
  fileName: string;
  contents: string;
  lastModified?: number;
};

type ActiveRequest = {
  requestId: string;
  kind: "prewarm" | "run" | "lint";
  resolve: (message: WorkerPrewarmResultMessage | WorkerRunResult | WorkerLintResult) => void;
  reject: (error: unknown) => void;
  onStatus?: (message: WorkerStatusMessage) => void;
};

export class CoNESWorkerClient {
  private readonly workerUrl: string;
  private worker: Worker | null = null;
  private activeRequest: ActiveRequest | null = null;
  private prewarmPromise: Promise<WorkerPrewarmResultMessage> | null = null;

  constructor({ workerUrl = DEFAULT_WORKER_URL }: WorkerClientOptions = {}) {
    this.workerUrl = workerUrl;
  }

  prewarm({ onStatus }: { onStatus?: (message: WorkerStatusMessage) => void } = {}) {
    if (this.prewarmPromise) {
      return this.prewarmPromise;
    }

    const requestId = crypto.randomUUID();
    const startedAt = performance.now();

    this.prewarmPromise = new Promise<WorkerPrewarmResultMessage>((resolve, reject) => {
      this.activeRequest = { requestId, kind: "prewarm", resolve: resolve as ActiveRequest["resolve"], reject, onStatus };

      try {
        this.ensureWorker();
      } catch (error) {
        this.activeRequest = null;
        resolve(createPrewarmFailure(requestId, "worker-create-failed", error, startedAt));
        return;
      }

      this.worker?.postMessage({ type: "prewarm", requestId });
    }).then((result) => {
      if (result.type === "prewarm-failure") {
        this.prewarmPromise = null;
      }
      return result;
    });

    return this.prewarmPromise;
  }

  async run(file: File, { onStatus }: { onStatus?: (message: WorkerStatusMessage) => void } = {}) {
    if (this.activeRequest?.kind === "run" || this.activeRequest?.kind === "lint") {
      this.terminate();
    }

    const prewarm = await this.prewarm({ onStatus });
    if (prewarm.type === "prewarm-failure") {
      return {
        type: "failure",
        requestId: prewarm.requestId,
        exitCode: null,
        stdout: "",
        stderr: "",
        error: prewarm.error || {
          code: "prewarm-failed",
          message: "Runtime prewarm failed.",
        },
        timings: prewarm.timings,
      } satisfies WorkerRunResult;
    }

    const requestId = crypto.randomUUID();
    const startedAt = performance.now();

    return new Promise<WorkerRunResult>((resolve, reject) => {
      this.activeRequest = { requestId, kind: "run", resolve: resolve as ActiveRequest["resolve"], reject, onStatus };

      try {
        this.ensureWorker();
      } catch (error) {
        this.activeRequest = null;
        resolve(createClientFailure(requestId, "worker-create-failed", error, startedAt));
        return;
      }

      file.arrayBuffer()
        .then((bytes) => {
          this.postRunRequest(requestId, file, bytes);
        })
        .catch((error) => {
          this.finishWithFailure(createClientFailure(requestId, "file-read-failed", error, startedAt));
        });
    });
  }

  runSource({ fileName, contents, lastModified = Date.now() }: RunSourceInput, options: { onStatus?: (message: WorkerStatusMessage) => void } = {}) {
    const safeName = fileName.toLowerCase().endsWith(".cnes") ? fileName : `${fileName}.cnes`;
    const file = new File([contents], safeName, {
      type: "text/plain",
      lastModified,
    });

    return this.run(file, options);
  }

  async lint(file: File, { onStatus }: { onStatus?: (message: WorkerStatusMessage) => void } = {}) {
    if (this.activeRequest?.kind === "run") {
      return createClientFailure(crypto.randomUUID(), "request-cancelled", new Error("Lint skipped while a run is active."), performance.now());
    }

    if (this.activeRequest?.kind === "lint") {
      this.terminate();
    }

    const prewarm = await this.prewarm({ onStatus });
    if (prewarm.type === "prewarm-failure") {
      return {
        type: "failure",
        requestId: prewarm.requestId,
        exitCode: null,
        stdout: "",
        stderr: "",
        error: prewarm.error || {
          code: "prewarm-failed",
          message: "Runtime prewarm failed.",
        },
        timings: prewarm.timings,
      } satisfies WorkerLintResult;
    }

    const requestId = crypto.randomUUID();
    const startedAt = performance.now();

    return new Promise<WorkerLintResult>((resolve, reject) => {
      this.activeRequest = { requestId, kind: "lint", resolve: resolve as ActiveRequest["resolve"], reject, onStatus };

      try {
        this.ensureWorker();
      } catch (error) {
        this.activeRequest = null;
        resolve(createClientFailure(requestId, "worker-create-failed", error, startedAt));
        return;
      }

      file.arrayBuffer()
        .then((bytes) => {
          this.postLintRequest(requestId, file, bytes);
        })
        .catch((error) => {
          this.finishWithFailure(createClientFailure(requestId, "file-read-failed", error, startedAt));
        });
    });
  }

  lintSource({ fileName, contents, lastModified = Date.now() }: RunSourceInput, options: { onStatus?: (message: WorkerStatusMessage) => void } = {}) {
    const safeName = fileName.toLowerCase().endsWith(".cnes") ? fileName : `${fileName}.cnes`;
    const file = new File([contents], safeName, {
      type: "text/plain",
      lastModified,
    });

    return this.lint(file, options);
  }

  terminate() {
    const active = this.activeRequest;

    this.worker?.terminate();
    if (active) {
      if (active.kind === "prewarm") {
        active.resolve({
          type: "prewarm-failure",
          requestId: active.requestId,
          error: {
            code: "request-cancelled",
            message: "Runtime prewarm cancelled.",
          },
        });
      } else {
        active.resolve({
          type: "failure",
          requestId: active.requestId,
          exitCode: null,
          stdout: "",
          stderr: "",
          error: {
            code: "request-cancelled",
            message: active.kind === "lint" ? "Lint cancelled." : "Run cancelled.",
          },
        });
      }
    }
    this.worker = null;
    this.activeRequest = null;
    this.prewarmPromise = null;
  }

  private ensureWorker() {
    if (this.worker) {
      return;
    }

    this.worker = new Worker(this.workerUrl);

    this.worker.addEventListener("message", (event: MessageEvent) => {
      this.handleMessage(event.data);
    });

    this.worker.addEventListener("error", (event) => {
      const active = this.activeRequest;
      if (!active) {
        return;
      }

      if (active.kind === "prewarm") {
        this.finishPrewarm(createPrewarmFailure(
          active.requestId,
          "worker-error",
          new Error(event.message || "Worker failed to load or execute."),
          performance.now(),
        ));
        return;
      }

      this.finishWithFailure(
        createClientFailure(
          active.requestId,
          "worker-error",
          new Error(event.message || "Worker failed to load or execute."),
          performance.now(),
        ),
      );
    });
  }

  private postRunRequest(requestId: string, file: File, bytes: ArrayBuffer) {
    if (!this.worker) {
      throw new Error("Worker is not available.");
    }

    this.worker.postMessage(
      {
        type: "run",
        requestId,
        file: {
          name: file.name,
          size: file.size,
          lastModified: file.lastModified,
          bytes,
        },
        argv: [...runArgv],
      },
      [bytes],
    );
  }

  private postLintRequest(requestId: string, file: File, bytes: ArrayBuffer) {
    if (!this.worker) {
      throw new Error("Worker is not available.");
    }

    this.worker.postMessage(
      {
        type: "lint",
        requestId,
        file: {
          name: file.name,
          size: file.size,
          lastModified: file.lastModified,
          bytes,
        },
        argv: [...lintArgv],
      },
      [bytes],
    );
  }

  private handleMessage(message: unknown) {
    if (!message || !this.activeRequest) {
      return;
    }

    const maybeMessage = message as { requestId?: string; type?: string };
    if (maybeMessage.requestId !== this.activeRequest.requestId) {
      return;
    }

    if (maybeMessage.type === "status") {
      this.activeRequest.onStatus?.(message as WorkerStatusMessage);
      return;
    }

    if (maybeMessage.type === "prewarm-ready" || maybeMessage.type === "prewarm-failure") {
      const parsed = workerPrewarmResultSchema.safeParse(message);
      if (!parsed.success) {
        this.finishPrewarm(createPrewarmFailure(this.activeRequest.requestId, "worker-response-invalid", parsed.error, performance.now()));
        return;
      }

      this.finishPrewarm(parsed.data);
      return;
    }

    const startedAt = performance.now();
    const parsed = workerResultSchema.safeParse(message);
    if (!parsed.success) {
      this.finishWithFailure(createClientFailure(this.activeRequest.requestId, "worker-response-invalid", parsed.error, startedAt));
      return;
    }

    this.finish(normalizeResult(parsed.data, startedAt));
  }

  private finish(message: WorkerRunResult) {
    const active = this.activeRequest;
    if (!active) {
      return;
    }
    this.activeRequest = null;
    active.resolve(message);
  }

  private finishWithFailure(message: WorkerRunResult) {
    const active = this.activeRequest;
    if (!active) {
      return;
    }
    this.worker?.terminate();
    this.worker = null;
    this.activeRequest = null;
    this.prewarmPromise = null;
    active.resolve(message);
  }

  private finishPrewarm(message: WorkerPrewarmResultMessage) {
    const active = this.activeRequest;
    if (!active) {
      return;
    }
    this.activeRequest = null;
    active.resolve(message);
  }
}

function normalizeResult(message: ReturnType<typeof workerResultSchema.parse>, startedAt: number): WorkerRunResult {
  return {
    ...message,
    timings: message.timings || { wallMs: performance.now() - startedAt },
    stdout: message.stdout || "",
    stderr: message.stderr || "",
    error: message.type === "failure"
      ? {
          code: message.error?.code || "runtime-error",
          message: message.error?.message || "Worker returned a failure.",
        }
      : message.error,
  };
}

function createPrewarmFailure(requestId: string, code: string, error: unknown, startedAt: number): WorkerPrewarmResultMessage {
  return {
    type: "prewarm-failure",
    requestId,
    error: {
      code,
      message: error instanceof Error ? error.message : String(error),
    },
    timings: { wallMs: performance.now() - startedAt },
  };
}

function createClientFailure(requestId: string, code: string, error: unknown, startedAt: number): WorkerRunResult {
  return {
    type: "failure",
    requestId,
    exitCode: null,
    stdout: "",
    stderr: "",
    error: {
      code,
      message: error instanceof Error ? error.message : String(error),
    },
    timings: { wallMs: performance.now() - startedAt },
  };
}
