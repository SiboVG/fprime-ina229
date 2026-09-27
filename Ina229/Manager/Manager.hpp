#pragma once
#include "Ina229/Device/Device.hpp"
#include "Ina229/Manager/ManagerComponentAc.hpp"
namespace Ina229 {
class Manager final : public ManagerComponentBase {
  public:
    explicit Manager(const char *name);

  private:
    // All handlers run with the component mutex held (guarded ports and commands).
    DriverStatus configure_handler(FwIndexType, const DeviceConfig &) override;
    DriverStatus read_handler(FwIndexType, Reading &) override;
    void INITIALIZE_cmdHandler(FwOpcodeType, U32, DeviceConfig) override;
    void CONFIGURE_ADC_cmdHandler(FwOpcodeType, U32, U8, U8, U8, U8, U8) override;
    void TRIGGER_cmdHandler(FwOpcodeType, U32) override;
    void STOP_cmdHandler(FwOpcodeType, U32) override;
    void RESET_ACCUMULATORS_cmdHandler(FwOpcodeType, U32) override;
    void SET_ALERT_CONFIG_cmdHandler(FwOpcodeType, U32, U16) override;
    void SET_THRESHOLD_cmdHandler(FwOpcodeType, U32, U8, U16) override;
    void READ_SNAPSHOT_cmdHandler(FwOpcodeType, U32) override;
    void REPORT_cmdHandler(FwOpcodeType, U32) override;
    void CLEAR_EVENT_THROTTLE_cmdHandler(FwOpcodeType, U32) override;

    class PortBus final : public SpiBus {
      public:
        explicit PortBus(Manager &owner) : m_owner(owner) {}
        bool transfer(const std::uint8_t *, std::uint8_t *, std::size_t) override;

      private:
        Manager &m_owner;
    };
    class OsDelay final : public Delay {
      public:
        bool milliseconds(std::uint32_t) override;
    };
    static DriverStatus convert(Status);
    // Unlocked helpers: callers already hold the (non-recursive) component mutex.
    Status initialize(const DeviceConfig &);
    Status acquire(Reading &);
    void record(Operation, Status);
    void recordValidity(U8 valid, U8 expected);
    void respond(FwOpcodeType, U32, Operation, Status);
    void clearThrottles();
    PortBus m_bus;
    OsDelay m_delay;
    Device m_device;
    Status m_last = Status::OK;
    U64 m_samples = 0, m_errors = 0;
    bool m_degraded = false;
    U8 m_degradedMask = 0;
};
} // namespace Ina229
