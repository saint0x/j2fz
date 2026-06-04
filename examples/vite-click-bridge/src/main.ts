import "./style.css";

interface ClickBridgeResponse {
  clickCount: number;
  inputCount: number;
  jsDoubled: number;
  nativeLifted: number;
  handshakeScore: number;
  bridgeModule: string;
}

const button = document.querySelector<HTMLButtonElement>("#click-button");
const status = document.querySelector<HTMLDivElement>("#status");
const inputCount = document.querySelector<HTMLParagraphElement>("#input-count");
const jsDoubled = document.querySelector<HTMLParagraphElement>("#js-doubled");
const nativeLifted = document.querySelector<HTMLParagraphElement>("#native-lifted");
const handshakeScore = document.querySelector<HTMLParagraphElement>("#handshake-score");
const payload = document.querySelector<HTMLPreElement>("#payload");

if (
  !button ||
  !status ||
  !inputCount ||
  !jsDoubled ||
  !nativeLifted ||
  !handshakeScore ||
  !payload
) {
  throw new Error("example UI failed to initialize");
}

function render(data: ClickBridgeResponse): void {
  inputCount.textContent = String(data.inputCount);
  jsDoubled.textContent = String(data.jsDoubled);
  nativeLifted.textContent = String(data.nativeLifted);
  handshakeScore.textContent = String(data.handshakeScore);
  payload.textContent = JSON.stringify(data, null, 2);
}

async function runBridgeClick(): Promise<void> {
  button.disabled = true;
  status.textContent = "Crossing the bridge...";
  try {
    const response = await fetch("/api/click", {
      method: "POST",
      headers: {
        "content-type": "application/json",
      },
      body: JSON.stringify({}),
    });
    if (!response.ok) {
      throw new Error(`request failed with ${response.status}`);
    }
    const data = (await response.json()) as ClickBridgeResponse;
    render(data);
    status.textContent = `Completed click ${data.clickCount} through ${data.bridgeModule}`;
  } catch (error) {
    status.textContent = error instanceof Error ? error.message : String(error);
  } finally {
    button.disabled = false;
  }
}

button.addEventListener("click", () => {
  void runBridgeClick();
});
