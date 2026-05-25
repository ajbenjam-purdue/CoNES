import { Badge } from "@/components/ui/badge";
import { Separator } from "@/components/ui/separator";

type PlaceholderPageProps = {
  label: string;
  description: string;
};

export function PlaceholderPage({ label, description }: PlaceholderPageProps) {
  return (
    <section className="flex h-full min-h-[calc(100svh-4rem)] flex-col gap-6 p-6">
      <div className="flex flex-col gap-2">
        <div className="flex items-center gap-2">
          <Badge variant="outline">Structured placeholder</Badge>
          <Badge variant="secondary">Post-Packet 003</Badge>
        </div>
        <h2 className="text-2xl font-semibold tracking-normal">{label}</h2>
        <p className="max-w-2xl text-sm text-muted-foreground">{description}</p>
      </div>

      <Separator />

      <div className="grid max-w-3xl gap-3 text-sm">
        <p>
          This lane is intentionally thin in Packet 003. It establishes navigation ownership without expanding scope before the
          React runner migration is accepted.
        </p>
        <p className="text-muted-foreground">
          Future packets can add CLI flags, lint diagnostics, runtime tools, and docs content while keeping solve execution in the
          browser worker.
        </p>
      </div>
    </section>
  );
}
