import { Button } from "@/components/ui/button"
import { IconDatabase, IconBolt, IconBrandGithub } from "@tabler/icons-react"

export default function App() {
  return (
    <div className="min-h-screen flex flex-col items-center justify-center p-6 gap-6">
      <div className="flex items-center gap-3">
        <IconDatabase className="size-10 text-primary" stroke={1.5} />
        <h1 className="text-4xl font-bold tracking-tight m-0">kvllay</h1>
      </div>
      <p className="text-muted-foreground text-center max-w-md">
        Lightweight, in-memory key-value database with Redis RESP2 protocol support.
      </p>
      <div className="flex items-center gap-3">
        <Button className="gap-2">
          <IconBolt className="size-4" stroke={2} />
          Get Started
        </Button>
        <Button variant="outline" className="gap-2">
          <IconBrandGithub className="size-4" stroke={2} />
          GitHub
        </Button>
      </div>
    </div>
  )
}