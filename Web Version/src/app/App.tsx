import { useEffect, useState } from "react";
import { AppShell } from "@/app/AppShell";
import type { AppRouteId } from "@/app/routes";
import { DocsPage } from "@/features/docs";
import { startRuntimePrewarm } from "@/features/runtime/runtime-prewarm";
import { RuntimeStatusPage } from "@/features/tools/RuntimeStatusPage";
import { WorkbenchPage } from "@/features/workbench";
import { WorkspaceOverview } from "@/features/workspace";

export function App() {
  const [activeRoute, setActiveRoute] = useState<AppRouteId>("workbench");

  useEffect(() => {
    startRuntimePrewarm();
  }, []);

  return (
    <AppShell activeRoute={activeRoute} onRouteChange={setActiveRoute}>
      {activeRoute === "workbench" ? <WorkbenchPage /> : null}
      {activeRoute === "workspace" ? <WorkspaceOverview /> : null}
      {activeRoute === "tools" ? <RuntimeStatusPage /> : null}
      {activeRoute === "docs" ? <DocsPage /> : null}
    </AppShell>
  );
}
