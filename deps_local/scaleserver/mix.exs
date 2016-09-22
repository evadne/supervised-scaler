defmodule Resampler.Mixfile do
  use Mix.Project

  def project do
    [app: :resampler,
     version: "0.0.1",
     elixir: "~> 1.0",
     compilers: [:make, :elixir, :app],
     aliases: [clean: ["clean", "clean.make"]],
     deps: deps()]
  end
  
  def application do
    [
      applications: [:logger, :erlexec, :exexec, :poolboy],
    ]
  end
  
  defp deps do
    [
      {:exexec, "~> 0.0.1"},
      {:poolboy, "~> 1.5"}
    ]
  end
end

defmodule Mix.Tasks.Compile.Make do
  @shortdoc "Compiles helper in c_src"

  def run(_) do
    {result, _error_code} = System.cmd("make", [], stderr_to_stdout: true)
    Mix.shell.info result
    :ok
  end
end

defmodule Mix.Tasks.Clean.Make do
  @shortdoc "Cleans helper in c_src"

  def run(_) do
    {result, _error_code} = System.cmd("make", ['clean'], stderr_to_stdout: true)
    Mix.shell.info result
    :ok
  end
end
