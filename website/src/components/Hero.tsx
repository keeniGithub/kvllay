import { useState } from "react"
import { ArrowRight, Star, Download, Copy, Check } from "lucide-react"
import { useGithubStars, formatStars } from "@/lib/useGithubStars"

export function Hero() {
  const [copied, setCopied] = useState(false)
  const { stars, isLoading } = useGithubStars()
  const dockerCmd = "docker run -d -p 6379:6379 --name kvllay kenyka/kvllay:latest"

  const handleCopy = () => {
    navigator.clipboard.writeText(dockerCmd)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <section className="w-full flex flex-col items-center pt-10 pb-12 px-6 lg:px-10 max-w-[1240px] mx-auto text-center">
      <div className="inline-flex items-center gap-2 px-3.5 py-1.5 rounded-full bg-[#0E131F] border border-[#1E3B5C] mb-8 shadow-sm">
        <span className="w-2 h-2 rounded-full bg-[#00D2FF] animate-pulse" />
        <span className="text-xs font-mono text-[#00D2FF] font-semibold tracking-wide uppercase">
          KVLLAY V1.0.0 IS LIVE
        </span>
        <span className="text-[#546682] text-xs">•</span>
        <span className="text-xs text-[#8B9BB4]">Pure C++ In-Memory Engine</span>
      </div>

      <div className="relative w-36 sm:w-44 lg:w-48 h-auto mb-6 drop-shadow-[0_10px_35px_rgba(0,210,255,0.25)] transition-transform hover:scale-105 duration-300">
        <img
          src="/allay_header.webp"
          alt="Kvllay Allay Mascot"
          className="w-full h-auto object-contain"
        />
      </div>

      <h1 className="text-4xl sm:text-5xl lg:text-6xl font-extrabold text-[#F0F6FC] tracking-tight max-w-[840px] leading-[1.12] mb-5">
        Fast. Lean.{" "}
        <span className="text-transparent bg-clip-text bg-gradient-to-r from-[#00D2FF] via-[#38BDF8] to-[#00E5A3]">
          Redis-Compatible.
        </span>
      </h1>

      <p className="text-base sm:text-lg text-[#8B9BB4] max-w-[660px] leading-relaxed mb-9">
        A drop-in Redis RESP2 replacement written in modern C++17. Uses 90x less memory than Redis in Docker, starts in 2 ms, and speaks native Redis protocol.
      </p>

      <div className="flex flex-wrap items-center justify-center gap-3.5 mb-10 w-full max-w-[620px]">
        <a
          href="#quickstart"
          className="flex items-center justify-center gap-2 px-6 py-3.5 bg-[#00D2FF] hover:bg-[#00b8e6] text-[#08090E] rounded-xl text-[15px] font-bold transition-all shadow-[0_0_25px_rgba(0,210,255,0.25)] hover:shadow-[0_0_30px_rgba(0,210,255,0.4)] w-full sm:w-auto"
        >
          <span>Get Started</span>
          <ArrowRight className="size-4 stroke-[2.5]" />
        </a>

        <a
          href="https://github.com/keeniGithub/kvllay"
          target="_blank"
          rel="noreferrer"
          className="flex items-center justify-center gap-2.5 px-5.5 py-3.5 bg-[#0E131F] hover:bg-[#161D2E] border border-[#1E3B5C] rounded-xl text-[15px] font-semibold text-[#F0F6FC] transition-colors w-full sm:w-auto group"
        >
          <Star className="size-4.5 text-[#EAB308] fill-[#EAB308]" />
          <span>Star on GitHub</span>
          {isLoading ? (
            <span className="ml-1 w-9 h-5 rounded-full bg-[#1B2436] animate-pulse inline-block" />
          ) : (
            <span className="ml-1 px-2 py-0.5 rounded-full bg-[#1B2436] text-xs font-mono text-[#00D2FF] group-hover:bg-[#223049] transition-colors">
              {formatStars(stars)}
            </span>
          )}
        </a>

        <a
          href="#quickstart"
          className="flex items-center justify-center gap-2 px-5 py-3.5 bg-[#0E131F] hover:bg-[#161D2E] border border-[#1E3B5C] rounded-xl text-[15px] font-medium text-[#8B9BB4] hover:text-[#F0F6FC] transition-colors w-full sm:w-auto"
        >
          <Download className="size-4 text-[#8B9BB4]" />
          <span>Standalone Binaries</span>
        </a>
      </div>

      <div className="w-full max-w-[780px] flex items-center justify-between gap-3 px-4 sm:px-5 py-3 bg-[#0A0E17] border border-[#1B2436] rounded-xl text-xs sm:text-sm font-mono shadow-inner group">
        <div className="flex items-center gap-3 overflow-x-auto scrollbar-none py-0.5">
          <span className="text-[#00D2FF] font-bold select-none">$</span>
          <span className="text-[#F0F6FC] whitespace-nowrap">{dockerCmd}</span>
        </div>
        <button
          type="button"
          onClick={handleCopy}
          className="shrink-0 p-1.5 text-[#546682] hover:text-[#00D2FF] rounded-md transition-colors flex items-center gap-1.5 focus:outline-none"
          title="Copy to clipboard"
        >
          {copied ? (
            <>
              <Check className="size-4 text-[#00D2FF]" />
              <span className="text-[11px] text-[#00D2FF] font-sans font-medium hidden sm:inline">Copied!</span>
            </>
          ) : (
            <Copy className="size-4" />
          )}
        </button>
      </div>
    </section>
  )
}
