// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// Copyright (C) 2012-2016, Ryan P. Wilson
//
//      Authority FX, Inc.
//      www.authorityfx.com

#ifndef AFX_IMAGE_H_
#define AFX_IMAGE_H_

#ifdef _WIN32
  #include <malloc.h>
#else
  #include <stdlib.h>
#endif

#include <ipp.h>

#include <boost/ptr_container/ptr_list.hpp>

#include <vector>
#include <memory>
#include <cstdint>

#include "include/bounds.h"
#include "include/pixel.h"
#include "include/threading.h"
#include "include/attribute.h"

namespace afx {

template <class T> class ImageBase : public AttributeBase {
 public:
  ImageBase<T>() : ptr_(nullptr), pitch_(0), region_(afx::Bounds()) {}
  explicit ImageBase<T>(const afx::Bounds& region) : ptr_(nullptr), pitch_(0) {
    Allocate(region);
  }
  ImageBase<T>(unsigned int width, unsigned int height) : ptr_(nullptr), pitch_(0) {
    Allocate(width, height);
  }
  ImageBase<T>(const ImageBase<T>& other) : ptr_(nullptr), pitch_(0) {
    Copy(other);
  }
  ImageBase<T>(ImageBase<T>&& other) : ptr_(other.ptr_),
                                       pitch_(other.pitch_),
                                       region_(other.region_) {
    other.ptr_ = nullptr;
    other.pitch_ = 0;
    other.region_ = afx::Bounds();
  }
  ImageBase<T>& operator=(const ImageBase<T>& other) {
    Copy(other);
    return *this;
  }
  ImageBase<T>& operator=(ImageBase<T>&& other) {
    if (this != &other) {
      Deallocate();
      ptr_ = other.ptr_;
      pitch_ = other.pitch_;
      region_ = other.region_;
      other.ptr_ = nullptr;
      other.pitch_ = 0;
      other.region_ = afx::Bounds();
    }
    return *this;
  }
  ~ImageBase<T>() {
    Deallocate();
  }
  void Allocate(unsigned int width, unsigned int height) {
    Allocate(afx::Bounds(0, 0, width - 1, height - 1));
  }
  void Allocate(const afx::Bounds& region) {
    Deallocate();
    region_ = region;
    pitch_ = (((region_.GetWidth() * sizeof(T) + 63) / 64) * 64);
    #ifdef _WIN32
      ptr_ = reinterpret_cast<T*>(_aligned_malloc(64, pitch_ *  region_.GetHeight()));
    #else
      ptr_ = reinterpret_cast<T*>(aligned_alloc(64, pitch_ *  region_.GetHeight()));
    #endif
  }
  void Copy(const ImageBase<T>& other) {
    region_ = other.region_;
    Allocate(region_);
    for (int y = region_.y1(); y <= region_.y2(); ++y) {
      memcpy(GetPtr(region_.x1(), y), other.GetPtr(other.region_.x1(), y), region_.GetWidth() * sizeof(T));
    }
  }
  void Deallocate() {
    if (ptr_ != nullptr) {
      free(ptr_);
      ptr_ = nullptr;
    }
  }
  void MemCpyIn(const T* ptr, std::size_t pitch) {
    const T* source_ptr = ptr;
    T* dest_ptr = GetPtr(region_.x1(), region_.y1());
    std::size_t size = region_.GetWidth() * sizeof(T);
    for (int y = region_.y1(); y <= region_.y2(); ++y) {
      memcpy(dest_ptr, source_ptr, size);
      source_ptr = reinterpret_cast<const T*>((reinterpret_cast<const std::uint8_t*>(source_ptr) + pitch));
      dest_ptr = this->GetNextRow(dest_ptr);
    }
  }
  void MemCpyIn(const T* ptr, std::size_t pitch, afx::Bounds region) {
    const T* source_ptr = ptr;
    T* dest_ptr = GetPtr(region.x1(), region.y1());
    std::size_t size = region.GetWidth() * sizeof(T);
    for (int y = region.y1(); y <= region.y2(); ++y) {
      memcpy(dest_ptr, source_ptr, size);
      source_ptr = reinterpret_cast<const T*>((reinterpret_cast<const std::uint8_t*>(source_ptr) + pitch));
      dest_ptr = this->GetNextRow(dest_ptr);
    }
  }
  void MemCpyIn(const ImageBase<T>& source_image, afx::Bounds region) {
    MemCpyIn(source_image.GetPtr(region.x1(), region.y1()), source_image.GetPitch(), region);
  }
  void MemCpyIn(const ImageBase<T>& source_image) {
    afx::Bounds region = source_image.GetBounds().GetIntersection(region_);
    MemCpyIn(source_image.GetPtr(region.x1(), region.y1()), source_image.GetPitch(), region);
  }
  void MemCpyOut(T* ptr, std::size_t pitch) const {
    T* source_ptr = GetPtr(region_.x1(), region_.y1());
    T* dest_ptr = ptr;
    std::size_t size = region_.GetWidth() * sizeof(T);
    for (int y = region_.y1(); y <= region_.y2(); ++y) {
      memcpy(dest_ptr, source_ptr, size);
      source_ptr = this->GetNextRow(dest_ptr);
      dest_ptr = reinterpret_cast<T*>((reinterpret_cast<std::uint8_t*>(dest_ptr) + pitch));
    }
  }
  void MemCpyOut(T* ptr, std::size_t pitch, afx::Bounds region) const {
    T* source_ptr = GetPtr(region.x1(), region.y1());
    T* dest_ptr = ptr;
    std::size_t size = region.GetWidth() * sizeof(T);
    for (int y = region.y1(); y <= region.y2(); ++y) {
      memcpy(dest_ptr, source_ptr, size);
      source_ptr = this->GetNextRow(dest_ptr);
      dest_ptr = reinterpret_cast<T*>((reinterpret_cast<std::uint8_t*>(dest_ptr) + pitch));
    }
  }
  void MemCpyOut(const ImageBase<T>& dest_image, afx::Bounds region) const {
    MemCpyOut(dest_image.GetPtr(region.x1(), region.y1()), dest_image.GetPitch(), region);
  }
  void MemCpyOut(const ImageBase<T>& dest_image) const {
    afx::Bounds region = dest_image.GetBounds();
    MemCpyOut(dest_image.GetPtr(region.x1(), region.y1()), dest_image.GetPitch(), region);
  }
  T* GetPtr() const {
    return ptr_;
  }
  T* GetPtr(int x, int y) const {
    return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(ptr_) + (y - region_.y1()) * pitch_ + (x - region_.x1()) * sizeof(T));
  }
  T* GetPtrBnds(int x, int y) const {
    return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(ptr_) + (region_.ClampY(y) - region_.y1()) * pitch_ + (region_.ClampX(x) - region_.x1()) * sizeof(T));
  }
  T* GetNextRow(T* ptr) const {
    return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(ptr) + pitch_);
  }
  const T* GetNextRow(const T* ptr) const {
    return reinterpret_cast<const T*>(reinterpret_cast<const std::uint8_t*>(ptr) + pitch_);
  }
  T* GetPreviousRow(T* ptr) const {
    return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(ptr) - pitch_);
  }
  std::size_t GetPitch() const {
    return pitch_;
  }
  afx::Bounds GetBounds() const {
    return region_;
  }
  bool IsAllocated() const {
    if (ptr_ != nullptr) {
      return true;
    } else {
      return false;
    }
  }
  IppiSize GetSize() const {
    IppiSize size = {static_cast<int>(region_.GetWidth()), static_cast<int>(region_.GetHeight())};
    return size;
  }

private:
  T* ptr_;
  std::size_t pitch_;
  afx::Bounds region_;
};

typedef ImageBase<float> Image;

class ImageLayer {
public:
  void AddImage(const Bounds& region) {
    channels_.push_back(std::shared_ptr<Image>(new Image(region)));
  }
  void AddImage(const std::shared_ptr<Image>& image_ptr) {
    channels_.push_back(image_ptr);
  }
  void MoveImage(const std::shared_ptr<Image>& image_ptr) {
    channels_.push_back(boost::move(image_ptr));
  }
  Image* operator[](int channel) const {
    return GetChannel(channel);
  }
  Image* GetChannel(int channel) const {
    if (static_cast<unsigned int>(channel) > channels_.size() - 1) {
      throw std::runtime_error("afx::Image - channel does not exist");
    }
    return channels_[channel].get();
  }
  afx::Pixel<const float> GetPixel(int x, int y) const {
    afx::Pixel<const float> pixel(static_cast<unsigned int>(channels_.size()));
    for (unsigned int i = 0; i < channels_.size(); ++i) {
      const float* ptr = channels_[i].get()->GetPtr(x, y);
      pixel.SetPtr(ptr, i);
    }
    return pixel;
  }
  afx::Pixel<float> GetWritePixel(int x, int y) const {
    afx::Pixel<float> pixel(static_cast<unsigned int>(channels_.size()));
    for (unsigned int i = 0; i < channels_.size(); ++i) {
      float* ptr = channels_[i].get()->GetPtr(x, y);
      pixel.SetPtr(ptr, i);
    }
    return pixel;
  }
  void ChannelCount() const {
    channels_.size();
  }
private:
  std::vector<std::shared_ptr<Image> > channels_;
};

class ImageArray : public Array<ImageBase<float> > {
 public:
  void Add(const Bounds& region) {
    this->array_.push_back(new Image(region));
  }
};

}  //  namespace afx

#endif  // AFX_IMAGE_H_

