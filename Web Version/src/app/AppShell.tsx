import { Cpu } from "lucide-react";
import { showDebugSurfaces } from "@/app/debug-flags";
import { appRoutes, type AppRouteId } from "@/app/routes";
import { Button } from "@/components/ui/button";
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from "@/components/ui/tooltip";
import { cn } from "@/lib/utils";

type AppShellProps = {
  activeRoute: AppRouteId;
  onRouteChange: (route: AppRouteId) => void;
  children: React.ReactNode;
};

export function AppShell({ activeRoute, onRouteChange, children }: AppShellProps) {
  const visibleRoutes = appRoutes.filter((route) => showDebugSurfaces || !route.debugOnly);
  const showSidebar = showDebugSurfaces || visibleRoutes.length > 1;

  return (
    <TooltipProvider delayDuration={250}>
      <div className="min-h-svh bg-background text-foreground">
        <div className={cn("grid min-h-svh", showSidebar ? "grid-cols-[260px_minmax(0,1fr)] max-lg:grid-cols-1" : "grid-cols-1")}>
          {showSidebar ? (
            <aside className="border-r bg-sidebar text-sidebar-foreground max-lg:border-b max-lg:border-r-0">
              <div className="flex min-h-svh flex-col max-lg:min-h-0">
                <div className="flex h-16 items-center gap-3 px-4">
                  <div className="flex size-10 items-center justify-center rounded-md border bg-background text-primary">
                    <Cpu aria-hidden="true" />
                  </div>
                  <div className="min-w-0">
                    <p className="truncate text-sm font-semibold">CoNES Studio</p>
                  </div>
                </div>

                <nav className="flex flex-1 flex-col gap-1 px-3 pb-4 max-lg:flex-none max-lg:flex-row max-lg:overflow-x-auto max-lg:pb-3">
                  {visibleRoutes.map((route) => {
                    const Icon = route.icon;
                    const isActive = route.id === activeRoute;

                    return (
                      <Tooltip key={route.id}>
                        <TooltipTrigger asChild>
                          <Button
                            type="button"
                            variant={isActive ? "secondary" : "ghost"}
                            className={cn(
                              "h-11 justify-start gap-3 rounded-md px-3",
                              isActive && "bg-sidebar-accent text-sidebar-accent-foreground",
                            )}
                            data-route-trigger={route.id}
                            onClick={() => onRouteChange(route.id)}
                          >
                            <Icon data-icon="inline-start" aria-hidden="true" />
                            <span>{route.label}</span>
                          </Button>
                        </TooltipTrigger>
                        <TooltipContent side="right">
                          <p>{route.description}</p>
                        </TooltipContent>
                      </Tooltip>
                    );
                  })}
                </nav>
              </div>
            </aside>
          ) : null}

          <main className="flex min-w-0 flex-col">
            <div className="min-h-0 flex-1">
              {children}
            </div>
          </main>
        </div>
      </div>
    </TooltipProvider>
  );
}
