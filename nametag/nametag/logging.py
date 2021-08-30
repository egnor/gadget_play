import logging
import signal
import sys


class _LogFormatter(logging.Formatter):
    def format(self, record):
        m = record.getMessage()
        ml = m.lstrip()
        out = ml.rstrip()
        pre, post = m[: len(m) - len(ml)], ml[len(out) :]
        if record.name != "root":
            out = f"{record.name}: {out}"
        if record.levelno < logging.INFO:
            out = f"🕸  {out}"
        elif record.levelno >= logging.CRITICAL:
            out = f"💥 {out}"
        elif record.levelno >= logging.ERROR:
            out = f"🔥 {out}"
        elif record.levelno >= logging.WARNING:
            out = f"⚠️ {out}"
        else:
            out = f"ℹ️ {out}"
        if record.exc_info and not record.exc_text:
            record.exc_text = self.formatException(record.exc_info)
        if record.exc_text:
            out = f"{out.strip()}\n{record.exc_text}"
        if record.stack_info:
            out = f"{out.strip()}\n{record.stack_info}"
        return pre + out.strip() + post


# Initialize on import.
signal.signal(signal.SIGINT, signal.SIG_DFL)
_log_handler = logging.StreamHandler(stream=sys.stderr)
_log_handler.setFormatter(_LogFormatter())
logging.basicConfig(level=logging.INFO, handlers=[_log_handler])
