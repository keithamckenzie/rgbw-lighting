export function isNearBottom(container: HTMLElement, threshold = 16): boolean {
  const distanceFromBottom = container.scrollHeight - container.scrollTop - container.clientHeight;
  return distanceFromBottom <= threshold;
}

export function scrollToBottom(container: HTMLElement): void {
  container.scrollTop = container.scrollHeight;
}

export function autoScroll(
  container: HTMLElement,
  params: { enabled?: boolean; resetKey?: string | number | null; threshold?: number } = {}
) {
  let shouldAutoScroll = true;
  let lastResetKey = params.resetKey;
  let threshold = params.threshold ?? 16;

  function handleScroll() {
    shouldAutoScroll = isNearBottom(container, threshold);
  }

  function maybeScroll(enabled: boolean | undefined) {
    if (enabled !== false && shouldAutoScroll) {
      scrollToBottom(container);
    }
  }

  container.addEventListener('scroll', handleScroll);

  maybeScroll(params.enabled);

  return {
    update(next: { enabled?: boolean; resetKey?: string | number | null; threshold?: number }) {
      if (next.threshold !== undefined) {
        threshold = next.threshold;
      }
      if (next.resetKey !== lastResetKey) {
        shouldAutoScroll = true;
        lastResetKey = next.resetKey;
      }
      maybeScroll(next.enabled);
    },
    destroy() {
      container.removeEventListener('scroll', handleScroll);
    }
  };
}
