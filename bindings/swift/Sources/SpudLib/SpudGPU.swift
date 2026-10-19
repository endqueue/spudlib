//
//  SpudGPU.swift
//  SpudLib
//
//  Created by Nathan Moore on 9/4/26.
//

import CSpudLib
import Foundation

/// Swift error wrapping a non-success `SPUDRESULT` returned from spudlib.
public struct SpudError: Error, CustomStringConvertible {
    public let result: SPUDRESULT

    public var description: String {
        spudresult_str(result).map { String(cString: $0) }
            ?? "unknown SPUDRESULT"
    }
}

private func spudCheck(_ result: SPUDRESULT) throws {
    guard result == SPUD_SUCCESS else { throw SpudError(result: result) }
}

/// RAII wrapper around a `spudgpu_instance`: creates the instance in `init`,
/// destroys it in `deinit`, and turns `SPUDRESULT` codes into thrown errors.
public final class SpudGPUInstance {
    public let handle: spudgpu_instance
    public private(set) var devices: Array<SpudGPUDevice>

    public init(
        nativeAPI: SPUDGPU_NATIVE_API,
        applicationName: String,
        applicationVersion: UInt32 = 0,
        engineName: String = "",
        engineVersion: UInt32 = 0
    ) throws {
        var instance: spudgpu_instance? = nil
        try spudCheck(
            spudgpu_create_instance(
                nativeAPI,
                applicationName,
                applicationVersion,
                engineName,
                engineVersion,
                &instance
            )
        )
        guard let instance else {
            throw SpudError(result: SPUDRESULT_NULL_OUTPUT_PARAMETER)
        }
        handle = instance
        devices = []
        devices = try enumerateDevices()
    }

    deinit {
        spudgpu_destroy_instance(handle)
    }

    public func enumerateDevices() throws -> Array<SpudGPUDevice> {
        var devicesPtr: UnsafeMutablePointer<spudgpu_device?>? = nil
        var count: UInt32 = 0
        try spudCheck(spudgpu_enumerate_devices(handle, &devicesPtr, &count))
        guard let devicesPtr else { return [] }
        let result : Array<SpudGPUDevice> = Array<SpudGPUDevice>(unsafeUninitializedCapacity: Int(count)) {
            $1 = Int(count)
            for i in 0..<Int(count) {
                $0[i] = SpudGPUDevice(device: devicesPtr[i]!, instance: self)
            }
        }
        return result
    }
}

extension SPUDGPU_DEVICE_PROPERTIES {
    /// `description` is a fixed C `char[128]`; Swift imports it as a tuple,
    /// so this converts it to a real String on demand.
    public var descriptionString: String {
        var copy = self
        return withUnsafePointer(to: &copy.description) {
            $0.withMemoryRebound(to: CChar.self, capacity: 128) {
                String(cString: $0)
            }
        }
    }
}

public final class SpudGPUDevice {
    public let handle: spudgpu_device
    public let instance: SpudGPUInstance

    fileprivate init(device: spudgpu_device, instance: SpudGPUInstance) {
        self.handle = device
        self.instance = instance
    }

    deinit {
    }

    public func properties() throws -> SPUDGPU_DEVICE_PROPERTIES {
        var props = SPUDGPU_DEVICE_PROPERTIES()
        props.base.type = SPUD_STRUCT_TYPE_GPU_DEVICE_PROPERTIES
        try spudCheck(spudgpu_get_device_properties(handle, &props))
        return props
    }
}

public final class SpudGPUBuffer {
    public let handle: spudgpu_buffer
    public let device: SpudGPUDevice

    fileprivate init(buffer: spudgpu_buffer, device: SpudGPUDevice) {
        self.handle = buffer
        self.device = device
    }

    deinit {
        spudgpu_destroy_buffer(handle)
    }

    public func map(offset: UInt64, size: UInt64) throws -> UnsafeMutableRawPointer {
        var mapped: UnsafeMutableRawPointer? = nil
        try spudCheck(spudgpu_map_buffer(handle, offset, size, &mapped))
        guard let mapped else {
            throw SpudError(result: SPUDRESULT_NULL_OUTPUT_PARAMETER)
        }
        return mapped
    }
}
