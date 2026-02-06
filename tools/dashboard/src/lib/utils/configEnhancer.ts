// Merge static field metadata with parsed config.h data at runtime

import type { ConfigDefine, EnhancedDefine, EnumValue, ValueType } from '$lib/types/config';
import { FIELD_METADATA, toHumanLabel, FIELD_GROUPS } from '$lib/config/fieldMetadata';

/**
 * Enhance a single config define with human-readable metadata
 */
export function enhanceDefine(define: ConfigDefine): EnhancedDefine {
  const meta = FIELD_METADATA[define.name];

  // Determine group (defaults to Advanced if no metadata)
  const group = meta?.group ?? FIELD_GROUPS.ADVANCED;

  // Fields in Advanced group should default to advanced: true
  // This ensures unknown fields are hidden by the toggle
  const isAdvancedGroup = group === FIELD_GROUPS.ADVANCED;
  const advanced = meta?.advanced ?? isAdvancedGroup;

  // If we have enumDescriptions in metadata but the backend didn't detect enum_values,
  // convert to enum type and generate enum_values from the descriptions
  let valueType: ValueType = define.value_type;
  let enumValues: EnumValue[] | null = define.enum_values;

  if (meta?.enumDescriptions && !define.enum_values) {
    valueType = 'enum';
    enumValues = Object.entries(meta.enumDescriptions).map(([value, description]) => {
      // Extract a short label from the description (text before the colon)
      const colonIndex = description.indexOf(':');
      const label = colonIndex > 0 ? description.substring(0, colonIndex).trim() : description;
      return { value, label };
    });
  }

  return {
    ...define,
    value_type: valueType,
    enum_values: enumValues,
    label: meta?.label ?? toHumanLabel(define.name),
    description: meta?.description ?? define.description ?? null,
    group,
    min: meta?.min,
    max: meta?.max,
    advanced,
    helpUrl: meta?.helpUrl,
    enumDescriptions: meta?.enumDescriptions,
    readOnly: meta?.readOnly ?? false,
    derivedFrom: meta?.derivedFrom,
    unit: meta?.unit
  };
}

/**
 * Enhance an array of config defines and optionally group them
 */
export function enhanceDefines(defines: ConfigDefine[]): EnhancedDefine[] {
  return defines.map(enhanceDefine);
}

/**
 * Group enhanced defines by their group property
 */
export function groupDefines(
  defines: EnhancedDefine[]
): Map<string, EnhancedDefine[]> {
  const groups = new Map<string, EnhancedDefine[]>();

  // Define preferred group order
  const groupOrder = [
    FIELD_GROUPS.DISPLAY,
    FIELD_GROUPS.HARDWARE,
    FIELD_GROUPS.PINS,
    FIELD_GROUPS.AUDIO,
    FIELD_GROUPS.INPUT,
    FIELD_GROUPS.TIMING,
    FIELD_GROUPS.ADVANCED
  ];

  // Initialize groups in preferred order
  for (const groupName of groupOrder) {
    groups.set(groupName, []);
  }

  // Distribute defines into groups
  for (const define of defines) {
    const groupName = define.group;
    if (!groups.has(groupName)) {
      groups.set(groupName, []);
    }
    groups.get(groupName)!.push(define);
  }

  // Remove empty groups
  for (const [key, value] of groups) {
    if (value.length === 0) {
      groups.delete(key);
    }
  }

  return groups;
}

/**
 * Filter defines by advanced flag
 */
export function filterAdvanced(
  defines: EnhancedDefine[],
  showAdvanced: boolean
): EnhancedDefine[] {
  if (showAdvanced) {
    return defines;
  }
  return defines.filter((d) => !d.advanced);
}
