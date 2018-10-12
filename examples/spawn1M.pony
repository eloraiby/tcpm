use "time"

actor Main
  var n: USize = 1_000_000
  let start_us: U64
  let env: Env

  new create(env': Env) =>
    env = env'
    start_us = Time.micros()
    var i: USize = 0
    while (i < n) do
      Heart(this)
      i = i + 1
    end

  be signal() =>
    n = n - 1
    if (n == 0) then
      let end_us: U64 = Time.micros()
      let elapsed: U64 = end_us - start_us
      env.out.print("Microseconds elapsed: " + elapsed.string())
    end

actor Heart
  new create(m: Main) =>
    ok()
    m.signal()

  be ok() =>
    None
