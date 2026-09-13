import { useState } from "react"
import { ArrowRight, Menu, X, Star } from "lucide-react"
import { IconBrandGithub } from "@tabler/icons-react"
import logoImg from "@root/logo.png"
import { useGithubStars, formatStars } from "@/lib/useGithubStars"

export function Navbar() {
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false)
  const { stars, isLoading } = useGithubStars()

  const navLinks = [
    { name: "Why Kvllay", href: "#why" },
    { name: "Benchmarks", href: "#benchmarks" },
    { name: "Features", href: "#features" },
    { name: "Quickstart", href: "#quickstart" },
    { name: "Docs", href: "#docs" },
  ]

  return (
    <header className="sticky top-0 z-50 w-full bg-[#070A10]/90 backdrop-blur-md border-b border-[#1B2436]">
      <div className="max-w-[1240px] mx-auto px-6 lg:px-10 h-[76px] flex items-center justify-between">
        <a href="#" className="flex items-center gap-3 group focus:outline-none">
          <div className="w-[38px] h-[38px] rounded-lg overflow-hidden flex items-center justify-center transition-transform group-hover:scale-105">
            <img src={logoImg} alt="kvllay logo" className="w-full h-full object-contain" />
          </div>
          <span className="text-[22px] font-bold text-[#F0F6FC] tracking-tight">kvllay</span>
        </a>

        <nav className="hidden md:flex items-center gap-8">
          {navLinks.map((link) => (
            <a
              key={link.name}
              href={link.href}
              className="text-[14px] text-[#8B9BB4] hover:text-[#F0F6FC] transition-colors font-medium"
            >
              {link.name}
            </a>
          ))}
        </nav>

        <div className="hidden md:flex items-center gap-3.5">
          <a
            href="https://github.com/keeniGithub/kvllay"
            target="_blank"
            rel="noreferrer"
            className="flex items-center gap-2 px-3.5 py-2 bg-[#0E131F] hover:bg-[#161D2E] border border-[#1E3B5C] rounded-lg text-[13px] font-medium text-[#F0F6FC] transition-colors group"
          >
            <IconBrandGithub className="size-4 text-[#F0F6FC]" />
            <span>GitHub</span>
            {isLoading ? (
              <span className="w-8 h-4 rounded bg-[#1B2436] animate-pulse inline-block" />
            ) : (
              <span className="flex items-center gap-1 px-1.5 py-0.5 rounded bg-[#1B2436] text-[11px] font-mono text-[#00D2FF] group-hover:bg-[#223049] transition-colors">
                <Star className="size-3 fill-[#00D2FF] text-[#00D2FF]" />
                {formatStars(stars)}
              </span>
            )}
          </a>
          <a
            href="#quickstart"
            className="flex items-center gap-1.5 px-4.5 py-2 bg-[#00D2FF] hover:bg-[#00b8e6] text-[#08090E] rounded-lg text-[13px] font-bold transition-all shadow-[0_0_20px_rgba(0,210,255,0.25)] hover:shadow-[0_0_25px_rgba(0,210,255,0.4)]"
          >
            <span>Deploy Now</span>
            <ArrowRight className="size-3.5 stroke-[2.5]" />
          </a>
        </div>

        <button
          type="button"
          onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
          className="md:hidden p-2 text-[#8B9BB4] hover:text-[#F0F6FC] focus:outline-none"
          aria-label="Toggle Navigation Menu"
        >
          {mobileMenuOpen ? <X className="size-6" /> : <Menu className="size-6" />}
        </button>
      </div>

      {mobileMenuOpen && (
        <div className="md:hidden border-b border-[#1B2436] bg-[#0A0E17]/98 px-6 py-6 flex flex-col gap-4 animate-in slide-in-from-top-2 duration-200">
          <nav className="flex flex-col gap-3">
            {navLinks.map((link) => (
              <a
                key={link.name}
                href={link.href}
                onClick={() => setMobileMenuOpen(false)}
                className="text-[15px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors py-1.5 font-medium"
              >
                {link.name}
              </a>
            ))}
          </nav>
          <div className="pt-4 border-t border-[#1B2436] flex flex-col sm:flex-row gap-3">
            <a
              href="https://github.com/keeniGithub/kvllay"
              target="_blank"
              rel="noreferrer"
              className="flex items-center justify-center gap-2 px-4 py-2.5 bg-[#0E131F] border border-[#1E3B5C] rounded-lg text-[14px] font-medium text-[#F0F6FC]"
            >
              <IconBrandGithub className="size-4 text-[#F0F6FC]" />
              <span>Star on GitHub</span>
            </a>
            <a
              href="#quickstart"
              onClick={() => setMobileMenuOpen(false)}
              className="flex items-center justify-center gap-2 px-4 py-2.5 bg-[#00D2FF] text-[#08090E] rounded-lg text-[14px] font-bold"
            >
              <span>Deploy Now</span>
              <ArrowRight className="size-4 stroke-[2.5]" />
            </a>
          </div>
        </div>
      )}
    </header>
  )
}
