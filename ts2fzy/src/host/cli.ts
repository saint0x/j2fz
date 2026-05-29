import { startJsHostServer } from "./server.js";

async function main(): Promise<void> {
  const socketPath = process.argv[2];
  if (!socketPath) {
    throw new Error("usage: node cli.js <socketPath>");
  }
  const server = await startJsHostServer({ socketPath });
  const shutdown = async (): Promise<void> => {
    await server.close();
    process.exit(0);
  };
  process.once("SIGINT", () => {
    void shutdown();
  });
  process.once("SIGTERM", () => {
    void shutdown();
  });
  await new Promise<void>(() => {});
}

void main().catch((error) => {
  const message = error instanceof Error ? error.stack ?? error.message : String(error);
  process.stderr.write(`${message}\n`);
  process.exit(1);
});

