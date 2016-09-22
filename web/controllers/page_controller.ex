defmodule Scaler.PageController do
  use Scaler.Web, :controller

  def index(conn, _params) do
    render conn, "index.html"
  end
  
  def preview(conn, params) do
    start = System.monotonic_time()
    {:ok, image_path} = Resampler.request(params["image"].path, 512, 512)
    stop = System.monotonic_time()
    diff = System.convert_time_unit(stop - start, :native, :micro_seconds)
    {:ok, image_content} = File.read(image_path)
    base64 = Base.encode64(image_content)
    render conn, "preview.html", base64: base64, diff: formatted_diff(diff)
  end
  
  defp formatted_diff(diff) when diff > 1000, do: [diff |> div(1000) |> Integer.to_string, "ms"]
  defp formatted_diff(diff), do: [diff |> Integer.to_string, "µs"]
end
