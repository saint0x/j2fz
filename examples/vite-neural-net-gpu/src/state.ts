export type RenderMode = "class" | "ascii" | "neuron";

export interface PointRecord {
  x: number;
  y: number;
  classId: number;
}

export interface WeightGroup {
  name: string;
  values: number[];
}

export interface ModelStats {
  loss: number;
  accuracy: number;
  step: number;
  pointCount: number;
}

export interface LessonStep {
  id: string;
  title: string;
  body: string;
  focus: string[];
}

export interface BootstrapPayload {
  width: number;
  height: number;
  classOptions: Array<{ id: number; label: string; hex: string }>;
  steps: LessonStep[];
  weights: WeightGroup[];
  stats: ModelStats;
  points: PointRecord[];
  sourceTabs: Record<string, string>;
}

export interface RenderPayload {
  width: number;
  height: number;
  mode: RenderMode;
  frameBase64: string;
  points: PointRecord[];
  stats: ModelStats;
  weights: WeightGroup[];
}
