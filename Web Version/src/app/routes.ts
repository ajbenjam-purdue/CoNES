import {
  BookOpen,
  Hammer,
  PanelsTopLeft,
  LayoutDashboard,
  type LucideIcon,
} from "lucide-react";

export type AppRouteId = "workbench" | "workspace" | "tools" | "docs";

export type AppRoute = {
  id: AppRouteId;
  label: string;
  description: string;
  icon: LucideIcon;
  debugOnly?: boolean;
};

export const appRoutes: AppRoute[] = [
  {
    id: "workbench",
    label: "Workbench",
    description: "Edit, lint, and run the active CoNES buffer.",
    icon: PanelsTopLeft,
  },
  {
    id: "workspace",
    label: "Workspace",
    description: "Review the current IDE lanes and operating guardrails.",
    icon: LayoutDashboard,
    debugOnly: true,
  },
  {
    id: "tools",
    label: "Tools",
    description: "Inspect runtime assets, manifests, and debugging details.",
    icon: Hammer,
    debugOnly: true,
  },
  {
    id: "docs",
    label: "Docs",
    description: "Collect local guidance for examples, exports, and runtime limits.",
    icon: BookOpen,
    debugOnly: true,
  },
];
