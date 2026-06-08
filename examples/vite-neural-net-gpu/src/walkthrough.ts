import type { LessonStep } from "./state";

export function renderStepRail(
  container: HTMLElement,
  steps: LessonStep[],
  currentStepId: string,
  onSelect: (stepId: string) => void,
): void {
  container.innerHTML = "";
  for (const step of steps) {
    const button = document.createElement("button");
    button.className = step.id === currentStepId ? "lesson active" : "lesson";
    button.type = "button";
    button.textContent = step.title;
    button.addEventListener("click", () => onSelect(step.id));
    container.appendChild(button);
  }
}

export function highlightSource(source: string, focus: string[]): string {
  let rendered = source
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
  for (const token of focus) {
    rendered = rendered.replaceAll(token, `<mark>${token}</mark>`);
  }
  return rendered;
}
