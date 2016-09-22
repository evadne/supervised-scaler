defmodule Resampler.Worker do
  @serverPath Path.expand("../priv_dir/convert", "#{__DIR__}")
  
  def start_link([]) do
    :gen_server.start_link(__MODULE__, [], [])
  end

  def init(state) do
    Process.flag(:trap_exit, true)
    {:ok, server_pid, server_os_pid} = Exexec.run_link(@serverPath, %{pty: true, stdin: true, stdout: true, stderr: true})
    {:ok, {server_pid, server_os_pid}}
  end
  
  def handle_call({:resample, fromPath, toWidth, toHeight}, from, {server_pid, server_os_pid} = state) do
    Exexec.send server_pid, "#{toWidth} #{toHeight} #{fromPath}\n"
    receive do
      {:stderr, ^server_os_pid, message} = x ->
        {:reply, {:error, String.strip(message)}, state}
      {:stdout, ^server_os_pid, message} = x ->
        #
        # FIXME: in PTY mode there is no distinction between STDOUT and STDERR
        # and STDOUT does not get picked up if I remove PTY flag from erlexec
        # so this requires further investigation
        #
        # https://github.com/saleyn/erlexec/issues/41
        #
        if String.starts_with?(message, "ERROR") do
          {:reply, {:error, String.strip(message)}, state}
        else
          {:reply, {:ok, String.strip(message)}, state}
        end
      x ->
        IO.inspect x
    after 1000 * 5 ->
      {:reply, {:error, :timeout}, state}
    end
  end
end
