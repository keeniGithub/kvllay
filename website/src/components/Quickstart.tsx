import { useState } from "react"
import { Terminal, Download, Hammer, Copy, Check } from "lucide-react"

export function Quickstart() {
  const [copiedCard, setCopiedCard] = useState<number | null>(null)

  const handleCopy = (text: string, index: number) => {
    navigator.clipboard.writeText(text)
    setCopiedCard(index)
    setTimeout(() => setCopiedCard(null), 2000)
  }

  const dockerCode = `docker run -d --name kvllay \\
  -p 6379:6379 kenyka/kvllay:latest`

  const binaryCode = `./kvllay-linux-x86_64 -p 6379
.\\kvllay-windows-x86_64.exe`

  const sourceCode = `git clone github.com/keeniGithub/kvllay
cd kvllay && make compile
./build/kvllay -p 6379`

  return (
    <section id="quickstart" className="w-full flex flex-col items-center py-16 px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <div className="flex flex-col items-center text-center gap-3 mb-12">
        <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-[#00D2FF]/10 border border-[#00D2FF]/30">
          <span className="text-[11px] font-mono font-bold text-[#00D2FF] tracking-wider uppercase">
            DEPLOYMENT OPTIONS
          </span>
        </div>
        <h2 className="text-2xl sm:text-3xl lg:text-[38px] font-bold text-[#F0F6FC] tracking-tight">
          Ready in Seconds. Pick Your Workflow.
        </h2>
        <p className="text-sm sm:text-base text-[#8B9BB4] max-w-[780px] leading-[1.6]">
          From zero-config Docker scratch images to native standalone executables and single-command local compilation.
        </p>
      </div>

      <div className="w-full grid grid-cols-1 md:grid-cols-3 gap-5 items-stretch">
        <div className="bg-[#0E131F] border border-[#00D2FF] rounded-xl p-6 flex flex-col justify-between shadow-[0_0_30px_rgba(0,210,255,0.08)]">
          <div className="flex flex-col gap-3.5 mb-5">
            <div className="flex items-center justify-between">
              <div className="w-9 h-9 rounded-lg bg-[#00D2FF]/15 flex items-center justify-center">
                <Terminal className="size-5 text-[#00D2FF]" />
              </div>
              <span className="px-2.5 py-1 rounded-md bg-[#00D2FF] text-[#08090E] text-[10px] font-mono font-bold tracking-wider uppercase">
                RECOMMENDED
              </span>
            </div>
            <h3 className="text-lg font-bold text-[#F0F6FC]">Docker Container</h3>
            <p className="text-[13px] text-[#8B9BB4] leading-[1.55]">
              Ultra-lean 1.6 MB scratch image. Starts up instantly with zero host dependencies.
            </p>
          </div>

          <div className="relative bg-[#0B0E17] border border-[#1B2436] rounded-lg p-3.5 font-mono text-xs text-[#F0F6FC] flex flex-col gap-1.5 group">
            <div className="text-[#F0F6FC] whitespace-pre-wrap font-medium leading-relaxed">
              docker run -d --name kvllay \<br />
              <span className="text-[#00D2FF]">  -p 6379:6379 kenyka/kvllay:latest</span>
            </div>
            <button
              type="button"
              onClick={() => handleCopy(dockerCode, 1)}
              className="absolute top-2.5 right-2.5 p-1.5 text-[#546682] hover:text-[#00D2FF] transition-colors focus:outline-none"
              title="Copy code"
            >
              {copiedCard === 1 ? <Check className="size-4 text-[#00D2FF]" /> : <Copy className="size-4" />}
            </button>
          </div>
        </div>

        <div className="bg-[#0E131F] border border-[#1B2436] hover:border-[#1E3B5C] rounded-xl p-6 flex flex-col justify-between transition-colors shadow-sm">
          <div className="flex flex-col gap-3.5 mb-5">
            <div className="flex items-center justify-between">
              <div className="w-9 h-9 rounded-lg bg-[#0A0E17] border border-[#1B2436] flex items-center justify-center">
                <Download className="size-5 text-[#F0F6FC]" />
              </div>
              <span className="px-2 py-0.5 rounded-md bg-[#0A0E17] text-[#546682] text-[10px] font-mono font-medium tracking-wider uppercase">
                PORTABLE
              </span>
            </div>
            <h3 className="text-lg font-bold text-[#F0F6FC]">Standalone Binaries</h3>
            <p className="text-[13px] text-[#8B9BB4] leading-[1.55]">
              Pre-compiled single executables for Linux x86_64 and Windows x86_64 with zero shared libraries.
            </p>
          </div>

          <div className="flex flex-col gap-3">
            <a
              href="https://github.com/keeniGithub/kvllay/releases"
              target="_blank"
              rel="noreferrer"
              className="w-full flex items-center justify-center gap-2 py-2.5 px-4 bg-[#00D2FF] hover:bg-[#00b8e6] text-[#08090E] rounded-lg text-[13px] font-bold transition-all shadow-[0_0_20px_rgba(0,210,255,0.2)] hover:shadow-[0_0_25px_rgba(0,210,255,0.35)]"
            >
              <Download className="size-4 stroke-[2.5]" />
              <span>Download Binaries (x86_64)</span>
            </a>

            <div className="relative bg-[#0B0E17] border border-[#1B2436] rounded-lg p-3 font-mono text-xs text-[#F0F6FC] flex flex-col gap-1 group">
              <div className="text-[#F0F6FC]">./kvllay-linux-x86_64 -p 6379</div>
              <div className="text-[#F0F6FC]">.\kvllay-windows-x86_64.exe</div>
              <button
                type="button"
                onClick={() => handleCopy(binaryCode, 2)}
                className="absolute top-2.5 right-2.5 p-1.5 text-[#546682] hover:text-[#00D2FF] transition-colors focus:outline-none"
                title="Copy commands"
              >
                {copiedCard === 2 ? <Check className="size-4 text-[#00D2FF]" /> : <Copy className="size-4" />}
              </button>
            </div>
          </div>
        </div>

        <div className="bg-[#0E131F] border border-[#1B2436] hover:border-[#1E3B5C] rounded-xl p-6 flex flex-col justify-between transition-colors shadow-sm">
          <div className="flex flex-col gap-3.5 mb-5">
            <div className="flex items-center justify-between">
              <div className="w-9 h-9 rounded-lg bg-[#0A0E17] border border-[#1B2436] flex items-center justify-center">
                <Hammer className="size-5 text-[#F0F6FC]" />
              </div>
              <span className="px-2 py-0.5 rounded-md bg-[#0A0E17] text-[#546682] text-[10px] font-mono font-medium tracking-wider uppercase">
                C++17 SOURCE
              </span>
            </div>
            <h3 className="text-lg font-bold text-[#F0F6FC]">Build from Source</h3>
            <p className="text-[13px] text-[#8B9BB4] leading-[1.55]">
              Compile in seconds with standard g++ or clang++. No external packages or third-party libraries.
            </p>
          </div>

          <div className="relative bg-[#0B0E17] border border-[#1B2436] rounded-lg p-3.5 font-mono text-[11.5px] text-[#F0F6FC] flex flex-col gap-1.5 group">
            <div className="text-[#F0F6FC]">git clone github.com/keeniGithub/kvllay</div>
            <div className="text-[#F0F6FC]">cd kvllay &amp;&amp; make compile</div>
            <div className="text-[#00D2FF] font-medium">./build/kvllay -p 6379</div>
            <button
              type="button"
              onClick={() => handleCopy(sourceCode, 3)}
              className="absolute top-2.5 right-2.5 p-1.5 text-[#546682] hover:text-[#00D2FF] transition-colors focus:outline-none"
              title="Copy commands"
            >
              {copiedCard === 3 ? <Check className="size-4 text-[#00D2FF]" /> : <Copy className="size-4" />}
            </button>
          </div>
        </div>
      </div>
    </section>
  )
}
