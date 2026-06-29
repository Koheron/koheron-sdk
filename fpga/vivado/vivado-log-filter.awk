BEGIN {
  RED = "\033[31;1m"
  YELLOW = "\033[33;1m"
  GREEN = "\033[32;1m"
  MAGENTA = "\033[35;1m"
  CYAN = "\033[96;1m"
  RESET = "\033[0m"
}

/^#/ {
  print CYAN $0 RESET
  next
}

/ERROR:/ {
  sub(/ERROR:/, RED "&" RESET)
  print
  next
}

/CRITICAL WARNING:/ {
  sub(/CRITICAL WARNING:/, MAGENTA "&" RESET)
  print
  next
}

/WARNING:/ {
  sub(/WARNING:/, YELLOW "&" RESET)
  print
  next
}

/INFO:/ {
  sub(/INFO:/, GREEN "&" RESET)
  print
  next
}

{
  print
}
