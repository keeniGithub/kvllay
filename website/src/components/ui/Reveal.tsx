import { type CSSProperties, type ReactNode } from "react"
import { useScrollReveal } from "@/lib/useScrollReveal"

export type RevealDirection = "up" | "down" | "left" | "right" | "scale" | "fade"
export type RevealTag = "div" | "section" | "span" | "article" | "aside" | "header" | "footer"

export interface RevealProps {
  children: ReactNode
  className?: string
  delay?: number
  duration?: number
  direction?: RevealDirection
  distance?: number
  threshold?: number
  rootMargin?: string
  once?: boolean
  as?: RevealTag
  style?: CSSProperties
}

export function Reveal({
  children,
  className = "",
  delay = 0,
  duration = 650,
  direction = "up",
  distance = 24,
  threshold = 0.1,
  rootMargin = "0px 0px -40px 0px",
  once = true,
  as: Tag = "div",
  style = {},
}: RevealProps) {
  const { ref, isVisible } = useScrollReveal<HTMLDivElement>({ threshold, rootMargin, once })

  const getInitialTransform = () => {
    switch (direction) {
      case "up":
        return `translate3d(0, ${distance}px, 0)`
      case "down":
        return `translate3d(0, -${distance}px, 0)`
      case "left":
        return `translate3d(${distance}px, 0, 0)`
      case "right":
        return `translate3d(-${distance}px, 0, 0)`
      case "scale":
        return "scale(0.95)"
      case "fade":
      default:
        return "none"
    }
  }

  const computedStyle: CSSProperties = {
    ...style,
    opacity: isVisible ? 1 : 0,
    transform: isVisible ? "none" : getInitialTransform(),
    transition: `opacity ${duration}ms cubic-bezier(0.16, 1, 0.3, 1) ${delay}ms, transform ${duration}ms cubic-bezier(0.16, 1, 0.3, 1) ${delay}ms`,
    willChange: isVisible ? "auto" : "opacity, transform",
  }

  return (
    <Tag ref={ref as React.Ref<HTMLDivElement & HTMLElement>} className={className} style={computedStyle}>
      {children}
    </Tag>
  )
}
