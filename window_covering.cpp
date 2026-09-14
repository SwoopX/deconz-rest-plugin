/*
 * window_covering.cpp
 *
 *
 *
 *
 * ZigBee Home Automation Public Application Profile Document 05-3520-29
 * Chapter 9.3 Window Covering Cluster
 *
 * Cluster Id 0x0102 Window Covering Cluster
 * Attributes
 * 0x0000 enum8,      WindowCoveringType
 * 0x0003 unsinged16, CurrentPositionLift
 * 0x0004 unsinged16, CurrentPositionTilt
 * 0x0008 unsinged8,  CurrentPositionLiftPercentage
 * 0x0009 unsinged8,  CurrentPositionTiltPercentage
 * 0x000A bitmap8,    OperationalStatus (This attribute contains two bits which will be set while the motor is active)
 * 0x0011 unsinged16, InstalledClosedLimitLift (Specifies a bound for the bottom position (lift height), in centimeters)
 * 0x0013 unsinged16, InstalledClosedLimitTilt (Specifies a bound for the closed position (tilt angle), in units of 0.1°)
 * 0x0017 bitmap8,    Mode (bit0=if the motor direction is reversed, bit1=the device is in calibration, bit2=maintenance mode)
 *
 * Commands
 * 0x00 Move up/open, Move upwards, towards the fully open position.
 * 0x01 Move down/close, Move downwards, towards the fully closed position.
 * 0x02 Stop, Stop all motion.
 * 0x04 Go to Lift Value, Moves to the specified lift value. Unsigned 16-bit integer.
 * 0x05 Go to Lift Percentage, Moves to the specified lift percentage. Unsigned 8-bit integer.
 * 0x07 Go to Tilt Value, Move to the specified tilt value. Unsigned 16-bit integer.
 * 0x08 Go to Tilt Percentage, Move to the specified tilt percentage. Unsigned 8-bit integer.
 *
 *
 * Ubisys Shutter Control J1
 *
 * The calibration procedure is implemented as DDF write function "ubisys:j1calibrate",
 * see writeUbisysJ1Calibration() in device_access_fn.cpp.
 */

#include "de_web_plugin_private.h"
#include "product_match.h"

/*! Handle packets related to the ZCL Window Covering cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Window Covering command or attribute
 */
void DeRestPluginPrivate::handleWindowCoveringClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    // FIXME: You're only handling ZclReadAttributesResponse and ZclReportAttributes - no other commands
    //        why not call this from deCONZ::NodeEvent instead that has already parsed the payload

    Q_UNUSED(ind);

    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

    if (!lightNode)
    {
        // was no relevant node
        return;
    }

    deCONZ::NumericUnion numericValue{};
    quint16 attrid = 0x0000;
    quint8 attrTypeId = 0x00;
    quint8 attrValue = 0x00;
    quint8 status = 0x00;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    NodeValue::UpdateType updateType = NodeValue::UpdateInvalid;
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {
        updateType = NodeValue::UpdateByZclRead;
    }
    else if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
    {
        updateType = NodeValue::UpdateByZclReport;
    }

    const QString modelId = lightNode->modelId();

    // Read ZCL reporting and ZCL Read Attributes Response
    if (updateType != NodeValue::UpdateInvalid)
    {
        while (!stream.atEnd())
        {
            stream >> attrid;
            if (updateType == NodeValue::UpdateByZclRead)
            {
                stream >> status;  // Read Attribute Response status
                if (status != 0)
                {
                    return;
                }
            }
            stream >> attrTypeId;
            switch (attrTypeId)
            {
                case deCONZ::Zcl8BitData:
                case deCONZ::ZclBoolean:
                case deCONZ::Zcl8BitBitMap:
                case deCONZ::Zcl8BitUint:
                case deCONZ::Zcl8BitInt:
                case deCONZ::Zcl8BitEnum:
                    stream >> attrValue;
                    break;
                case deCONZ::Zcl16BitData:
                case deCONZ::Zcl16BitBitMap:
                case deCONZ::Zcl16BitUint:
                case deCONZ::Zcl16BitInt:
                case deCONZ::Zcl16BitEnum:
                    quint16 attrVal16;
                    stream >> attrVal16;
                    break;
                default:
                    // unsupported data type
                    return;
            }

            if (attrid == 0x0008) // current CurrentPositionLiftPercentage 0-100
            {
                // Update value in the GUI.
                numericValue.u8 = attrValue;
                lightNode->setZclValue(updateType, ind.srcEndpoint(), WINDOW_COVERING_CLUSTER_ID, attrid, numericValue);

                quint8 lift = attrValue;
                // Reverse value for somes curtains
                if (modelId.startsWith(QLatin1String("lumi.curtain")) ||
                    modelId == QLatin1String("D10110") ||
                    modelId == QLatin1String("Motor Controller"))
                {
                    lift = 100 - lift;
                }
                // Reverse value for Legrand but only for old value
                else if (modelId == QLatin1String("Shutter SW with level control") ||
                         modelId == QLatin1String("Shutter switch with neutral"))
                {
                    bool bStatus = false;
                    uint nHex = lightNode->swBuildId().toUInt(&bStatus,16);
                    if (bStatus && (nHex < 28))
                    {
                        lift = 100 - lift;
                    }
                }
                // Reverse for some tuya covering
                else if (R_GetProductId(lightNode) == QLatin1String("11830304 Switch") ||
                         R_GetProductId(lightNode) == QLatin1String("Zigbee curtain switch") ||
                         R_GetProductId(lightNode) == QLatin1String("Zigbee dual curtain switch") ||
                         R_GetProductId(lightNode) == QLatin1String("Covering Switch ESW-2ZAD-EU") ||
                         R_GetProductId(lightNode) == QLatin1String("QS-Zigbee-C01 Module"))
                {
                    lift = 100 - lift;
                }

                bool open = lift < 100;

                if (lightNode->setValue(RStateLift, lift))
                {
                    pushZclValueDb(lightNode->address().ext(), lightNode->haEndpoint().endpoint(), WINDOW_COVERING_CLUSTER_ID, attrid, attrValue);
                }
                lightNode->setValue(RStateOpen, open);

                // FIXME: deprecate
                quint8 level = lift * 254 / 100;
                bool on = level > 0;
                lightNode->setValue(RStateBri, level);
                lightNode->setValue(RStateOn, on);
                // END FIXME: deprecate
            }
            else if (attrid == 0x0009) // current CurrentPositionTiltPercentage 0-100
            {
                numericValue.u8 = attrValue;
                lightNode->setZclValue(updateType, ind.srcEndpoint(), WINDOW_COVERING_CLUSTER_ID, attrid, numericValue);

                quint8 tilt = attrValue;
                if (lightNode->setValue(RStateTilt, tilt))
                {
                    pushZclValueDb(lightNode->address().ext(), lightNode->haEndpoint().endpoint(), WINDOW_COVERING_CLUSTER_ID, attrid, attrValue);
                }

                // FIXME: deprecate
                quint8 sat = attrValue * 254 / 100;
                lightNode->setValue(RStateSat, sat);
                // END FIXME: deprecate
            }
        }
    }
}

/*! Adds a window covering task to the queue.

   \param task - the task item
   \param cmdId - moveUp/Down/stop/moveTo/moveToPct
   \param pos - position centimeter
   \param pct - position percent
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskWindowCovering(TaskItem &task, uint8_t cmd, uint16_t pos, uint8_t pct)
{
    task.taskType = TaskWindowCovering;

    task.req.setClusterId(WINDOW_COVERING_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(cmd);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    if (cmd == 0x04 || cmd == 0x05 || cmd == 0x07 || cmd == 0x08)
    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        if (cmd == 0x04 || cmd == 0x07)
        {
            stream << pos;  // 16-bit moveToPosition
        }
        if (cmd == 0x05 || cmd == 0x08)
        {
            stream << pct;  // 8-bit moveToPct
        }
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

bool DeRestPluginPrivate::addTaskWindowCoveringSetAttr(TaskItem &task, uint16_t mfrCode, uint16_t attrId, uint8_t attrType, uint16_t attrValue)
{
    DBG_Printf(DBG_INFO, "addTaskWindowCoveringSetAttr: mfrCode = 0x%04x, attrId = 0x%04x, attrType = 0x%02x, attrValue = 0x%04x\n", mfrCode, attrId, attrType, attrValue);

    task.taskType = TaskWindowCovering;

    task.req.setDstEndpoint(0x01);
    task.req.setClusterId(WINDOW_COVERING_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);

    task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                            deCONZ::ZclFCDirectionClientToServer |
                            deCONZ::ZclFCDisableDefaultResponse);
    if (mfrCode != 0x0000)
    {
        task.zclFrame.setFrameControl(task.zclFrame.frameControl() | deCONZ::ZclFCManufacturerSpecific);
        task.zclFrame.setManufacturerCode(mfrCode);
    }

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << (quint16) attrId;
        stream << (quint8) attrType;
        if (attrType == deCONZ::Zcl8BitEnum || attrType == deCONZ::Zcl8BitBitMap || attrType == deCONZ::Zcl8BitUint)
        {
            stream << (quint8) attrValue;
        }
        else if (attrType == deCONZ::Zcl16BitUint)
        {
            stream << (quint16) attrValue;
        }
        else
        {
            DBG_Printf(DBG_INFO, "unsupported attribute type 0x%04x\n", attrType);
            return false;
        }
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
