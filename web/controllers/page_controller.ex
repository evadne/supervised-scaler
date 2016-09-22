defmodule Scaler.PageController do
  use Scaler.Web, :controller

  def index(conn, _params) do
    render conn, "index.html"
  end
  
  def preview(conn, params) do
    
    path = params["image"].path
    {:ok, resampler_diff, resampler_base64} = process_resampler(path, 512, 512)
    {:ok, mogrify_diff, mogrify_base64} = process_mogrify(path, 512, 512)
    
    render conn, "preview.html",
      resampler_base64: resampler_base64,
      resampler_diff: formatted_diff(resampler_diff),
      mogrify_base64: mogrify_base64,
      mogrify_diff: formatted_diff(mogrify_diff)
  end
  
  defp process_mogrify(path, width, height) do
    start = System.monotonic_time()
    
    %Mogrify.Image{path: image_path} = Mogrify.open(path)
      |> Mogrify.resize_to_limit("#{width}x#{height}")
      |> Mogrify.save
    
    stop = System.monotonic_time()
    diff = System.convert_time_unit(stop - start, :native, :micro_seconds)
    {:ok, image_content} = File.read(image_path)
    base64 = Base.encode64(image_content)
    
    {:ok, diff, base64}
  end
  
  defp process_resampler(path, width, height) do
    start = System.monotonic_time()
    {:ok, image_path} = Resampler.request(path, 512, 512)
    stop = System.monotonic_time()
    diff = System.convert_time_unit(stop - start, :native, :micro_seconds)
    {:ok, image_content} = File.read(image_path)
    base64 = Base.encode64(image_content)
    
    {:ok, diff, base64}
  end
  
  defp formatted_diff(diff) when diff > 1000, do: [diff |> div(1000) |> Integer.to_string, "ms"]
  defp formatted_diff(diff), do: [diff |> Integer.to_string, "µs"]
end
