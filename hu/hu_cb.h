#include <algorithm> // for std::min
#include <deque>
#include <mutex>

class CircularBuffer
{
public:
  CircularBuffer();
  ~CircularBuffer();
  
  size_t size();
  // Return number of bytes written.
  size_t write(const char *data, size_t bytes);
  // Return number of bytes read.
  int read(uint8_t* buf, size_t max_len);

private:
  std::mutex mtx;
  std::deque<std::vector<uint8_t>> buffers;
};

CircularBuffer::CircularBuffer()
{}

CircularBuffer::~CircularBuffer()
{}

size_t CircularBuffer::write(const char *data, size_t bytes)
{
  if (data == nullptr || bytes == 0) return 0;
  std::lock_guard<std::mutex> lock(mtx);
  buffers.emplace_back(data, data + bytes);
  return bytes;
}

int CircularBuffer::read(uint8_t* buf, size_t max_len)
{
  std::lock_guard<std::mutex> lock(mtx);
  if (buffers.empty()) return 0;
  auto& buffer = buffers.front();
  int to_copy = std::min(max_len, buffer.size());
  memcpy(buf, buffer.data(), to_copy);
  buffers.pop_front();
  return to_copy;
}

size_t CircularBuffer::size() 
{
  return buffers.size();
}