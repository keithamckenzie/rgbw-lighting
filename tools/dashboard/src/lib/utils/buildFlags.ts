export function quoteBuildFlagValue(value: string): string {
  if (value.length === 0) {
    return '""';
  }

  const trimmed = value.trim();
  // If quotes are mismatched, we treat them as literal content and re-quote safely below.
  const alreadyQuoted =
    (trimmed.startsWith('"') && trimmed.endsWith('"')) ||
    (trimmed.startsWith("'") && trimmed.endsWith("'"));

  if (alreadyQuoted) {
    return trimmed;
  }

  const needsQuotes = /\s|"/.test(value);
  if (!needsQuotes) {
    return value;
  }

  const escaped = value
    .replace(/\\/g, '\\\\')
    .replace(/"/g, '\\"')
    .replace(/\$/g, '\\$')
    .replace(/`/g, '\\`');
  return `"${escaped}"`;
}

export function buildFlagsFromDefines(defines: Record<string, string>): string[] {
  return Object.entries(defines).map(([name, value]) => `-D${name}=${quoteBuildFlagValue(value)}`);
}
