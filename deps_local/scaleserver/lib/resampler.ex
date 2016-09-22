defmodule Resampler do
  def request(fromPath, toWidth, toHeight) do
    Resampler.Pool.request(fromPath, toWidth, toHeight)
  end
end
