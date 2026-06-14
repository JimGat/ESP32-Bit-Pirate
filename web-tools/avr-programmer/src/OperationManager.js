export class OperationManager {
  constructor({ onState = () => {} } = {}) {
    this.busy = false;
    this.onState = onState;
  }

  async run(label, task) {
    if (this.busy) {
      throw new Error("Another AVR operation is already running.");
    }

    this.busy = true;
    this.onState({ busy: true, label, progress: 8 });
    const started = performance.now();
    try {
      const result = await task({
        step: (stepLabel, progress) => this.onState({ busy: true, label: stepLabel, progress }),
      });
      this.onState({ busy: false, label: "Complete", progress: 100, duration: performance.now() - started });
      return result;
    } catch (error) {
      const cancelled = error?.cancelled === true;
      this.onState({
        busy: false,
        label: cancelled ? "Cancelled" : "Error",
        progress: 0,
        error: cancelled ? null : error,
        cancelled,
      });
      throw error;
    } finally {
      this.busy = false;
    }
  }
}
