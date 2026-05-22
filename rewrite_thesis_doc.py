from __future__ import annotations

import shutil
from pathlib import Path
from textwrap import dedent

from docx import Document
from docx.enum.text import WD_BREAK


ABSTRACT_CN = dedent(
    """
    摘  要

    针对灾后临时安置点在环境监测、水电管理和异常处置方面存在的信息获取不及时、管理链路分散以及现场自治能力不足等问题，设计并实现了一套基于 STM32、ESP32、MQTT、Qt、Web 与 SQLite 的智慧水电管理与环境监测系统。系统以 STM32F103 为现场主控节点，完成温湿度、空气质量、PM2.5、电流和水流等多源数据采集，并在本地实现三级预警、组合联动和执行器控制；以 ESP32 作为通信网关，通过 UART 与 MQTT 建立现场节点与上位端之间的双向通信链路；以 Qt 桌面端作为管理中心，实现实时监测、阈值配置、远程控制、报警管理、历史查询和用户管理；以 Web 看板作为轻量展示终端，实现实时可视化和一键求助功能；以 SQLite 构建本地数据闭环，实现历史数据、报警事件、离线记录和远程操作日志的持久化管理。

    在系统设计过程中，结合真实源码构建了统一 JSON 数据模型，并围绕 `sen`、`res`、`lv`、`act`、`mod`、`alm`、`actn` 和 `flt` 等字段组织上行状态快照，保证了 STM32、ESP32、Qt 和 Web 多端之间的数据一致性。针对同主题双向通信场景，系统在 ESP32 网关侧实现了消息分类转发与回环抑制机制；针对现场异常处置需求，系统在 STM32 端实现了基于阈值比较、连续确认和滞回恢复的三级状态机，并进一步支持多指标组合联动。测试结果表明，该系统能够稳定完成多传感器采集、MQTT 双向通信、多端一致显示、本地数据存储和远程控制功能，基本满足灾后临时安置点环境监测与水电管理的应用需求。

    关键词：灾后临时安置点；物联网；环境监测；水电管理；MQTT；Qt；SQLite
    """
).strip()


ABSTRACT_EN = dedent(
    """
    ABSTRACT

    To address the problems of untimely information acquisition, fragmented management links, and insufficient on-site autonomy in environmental monitoring and water-electricity management for temporary post-disaster shelters, this thesis designs and implements an intelligent monitoring and management system based on STM32, ESP32, MQTT, Qt, Web, and SQLite. The system uses STM32F103 as the on-site main controller to collect temperature, humidity, air quality, PM2.5, current, and water-flow data, and to execute three-level warning logic, combined linkage control, and actuator response locally. ESP32 is used as the communication gateway to establish a bidirectional communication chain between the field node and upper terminals through UART and MQTT. The Qt desktop client functions as the management center for real-time monitoring, threshold configuration, remote control, alarm management, history query, and user management. The Web dashboard acts as a lightweight presentation terminal for real-time visualization and one-click help reporting. SQLite is adopted to build a local data loop for persistent storage of historical samples, alarm events, offline records, and remote operation logs.

    During implementation, a unified JSON message model is constructed based on the real source code, and the uplink state snapshot is organized with fields such as `sen`, `res`, `lv`, `act`, `mod`, `alm`, `actn`, and `flt`, which ensures data consistency among STM32, ESP32, Qt, and Web terminals. For the bidirectional communication under a shared MQTT topic, the ESP32 gateway implements message classification and loop prevention. For on-site abnormal handling, the STM32 side adopts a three-level state machine based on threshold comparison, consecutive confirmation, and hysteresis recovery, and further supports multi-index combined linkage. Test results show that the system can stably accomplish multi-sensor acquisition, MQTT bidirectional communication, multi-terminal consistent display, local data persistence, and remote control, which basically meets the application requirements of environmental monitoring and water-electricity management in temporary post-disaster shelters.

    Key words: temporary post-disaster shelter; Internet of Things; environmental monitoring; water and electricity management; MQTT; Qt; SQLite
    """
).strip()


BODY_MD = dedent(
    """
    # 第 1 章 绪 论

    ## 1.1 本文工作的来源

    灾后临时安置点在应急救援和过渡安置阶段承担着集中居住、生活保障和临时公共服务等多重功能。相关统计和研究表明，自然灾害发生后，安置点的环境安全、基础供给和管理效率会直接影响安置秩序与受灾群众生活质量[1][2]。

    在这一背景下，安置点内空气质量、温湿度、水电使用状态等信息如果仅依赖人工巡查，往往存在发现滞后、记录分散和处置不及时等问题。已有研究指出，安置点空气质量及其健康风险具有较强的场景敏感性，需要借助持续监测手段进行动态掌握[3]。因此，本文工作的来源并非抽象的技术选题，而是面向灾后临时安置场景中环境监测与水电管理的实际需求。

    结合当前项目真实源码，本文实现的系统以 STM32F103 为现场主控单元，配合 ESP32 通信网关、MQTT 消息机制、Qt 桌面管理端、Web 看板以及 SQLite 本地数据库，构建了一个面向灾后临时安置点的智慧水电管理与环境监测系统。该系统能够对温湿度、空气质量、PM2.5、电流和水流等关键指标进行实时监测，并在状态异常时触发本地联动控制，同时支持多端展示、远程干预和本地数据留存。

    ## 1.2 目的和意义

    本文研究的直接目的，是设计并实现一套面向灾后临时安置点的轻量化物联网监测与管理系统，使现场环境状态、水电使用状态和异常事件能够被实时感知、及时上报并形成可追踪记录。物联网技术的发展为分布式感知与远程协同提供了实现基础，而发布/订阅通信机制则有助于降低多节点交互复杂度[4][5]。

    从应用角度看，系统通过对温湿度、PM2.5、空气质量、电流和水流等指标进行统一监测，并结合蜂鸣器、风扇、舵机和指示灯等执行器实现本地响应，有助于减少单纯依赖人工值守所带来的滞后性，提高异常发现与处置效率。该设计面向的是“可快速部署、可本地自治、可远程查看”的实际使用需求，而不是大规模云平台型部署。

    从技术角度看，本文的意义主要体现在三个方面。第一，系统采用 STM32、ESP32、MQTT、Qt、Web 与 SQLite 的轻量协同架构，适合资源受限场景下的快速落地。第二，系统以统一 JSON 报文承载感知数据、状态信息和控制信息，能够增强多端状态一致性。第三，系统不依赖独立后端服务，而是通过 Qt 与 SQLite 建立本地数据闭环，这对于低成本、小规模物联网系统设计具有一定参考价值。相关环境监测系统研究同样表明，围绕物联网感知节点构建完整的数据采集与管理链路，能够提升系统在实际场景中的应用价值[2]。

    ## 1.3 国内外进展

    随着物联网、嵌入式系统、无线通信和可视化技术的发展，环境监测系统已经由单机采集逐渐演进为“感知采集—网络传输—平台处理—多端展示”的综合体系。MQTT 等轻量级协议因其发布/订阅机制和较低通信开销，被广泛应用于资源受限设备之间的消息交互[5][6]。在此基础上，多终端协同、云边协同和本地闭环控制成为当前监测系统的重要发展方向。

    ### 1.3.1 国外进展

    国外相关研究首先体现为云平台型物联网系统的成熟发展。此类方案通常以统一消息总线和设备管理平台为核心，强调海量设备接入、跨区域分发和集中式维护。相关协议综述指出，面向物联网的应用层协议正在围绕轻量化、可靠传输和平台解耦不断演化，其中发布/订阅机制在复杂节点协同中具有明显优势[6]。

    其次，国外在工业级监测系统和边缘监测节点方面发展较快。相关研究普遍重视高可靠采集、边缘预处理和持续在线能力，尤其是在低成本环境监测站与空气质量评估场景中，传感器节点往往能够在本地完成部分处理后再上传数据[7]。这说明国外方案在系统连续性、部署成熟度和工程规范性方面已经形成较完整路线。

    另外，国外系统在大规模部署、标准化接口和平台生态方面具有明显优势，但其常见方案往往依赖成熟云平台、专用设备管理能力或较高部署成本。对于灾后临时安置点这类规模较小、场地变化较快、强调本地自治和快速部署的场景而言，这类方案虽然技术成熟，却未必具有最佳适配性。

    ### 1.3.2 国内进展

    国内相关研究中，基于 STM32 的中小型环境监测系统较为常见。许多研究采用温湿度、气体、颗粒物等传感器构建现场采集节点，并在单片机端实现基础显示与报警功能[8][10]。这类方案结构清晰、成本较低，适合课程设计、毕业设计和中小规模原型验证，但在多端协同和统一消息模型方面仍有提升空间。

    在通信与平台侧，MQTT 已成为国内物联网场景中应用较广的协议之一。相关研究表明，MQTT 在远程监测系统中能够有效降低设备间耦合度，并适应嵌入式环境下的轻量通信需求[9]。与此同时，基于 ESP32 的远程控制与网关方案也日益常见，常用于把串口侧采集节点接入网络消息平台[12]。

    在上位机与可视化方面，Qt 与 Web 技术的组合逐渐成为国内小型物联网系统的重要实现方式。基于 Qt 的远程监控系统能够较好地承担桌面端的实时监控与控制任务[11]；基于云平台或浏览器端的监测系统则更强调轻量展示与远程访问能力[13][14]。总体来看，国内方案在功能实现上已经较为丰富，但不少系统仍存在感知端、通信端、桌面端和展示端分散实现的问题，多端状态一致性和本地数据闭环能力仍有待加强。

    ## 1.4 本文工作的主要内容

    基于上述背景与现状，本文围绕灾后临时安置点智慧水电管理与环境监测需求，结合真实项目源码，完成了一个轻量化多端协同系统的设计与实现。本文的主要工作包括以下几个方面。

    第一，设计了以 STM32F103 为核心的现场控制节点，实现温湿度、空气质量、PM2.5、电流和水流等数据采集，并在本地完成三级预警判定、组合联动和执行器控制。第二，构建了基于 ESP32 和 MQTT 的双向通信链路，实现现场状态上报与远程控制消息转发。第三，完成了 Qt 桌面客户端和 Web 看板的多端实现，使系统同时具备管理型界面与轻量展示界面。第四，建立了基于 SQLite 的本地数据闭环，实现历史数据、报警信息、远程控制日志、离线记录和用户信息的持久化管理。第五，对系统进行了功能、通信、稳定性和存储可靠性测试，验证了其设计方案的可行性。

    本文后续章节安排如下：第二章对系统需求进行分析；第三章阐述系统总体设计；第四章对硬件、STM32、ESP32、Qt、Web 和数据库的详细实现进行说明；第五章对系统测试过程和测试结果进行分析。

    [[PAGEBREAK]]
    # 第 2 章 需求分析

    ## 2.1 功能性需求

    本系统的功能性需求围绕“现场感知、状态判定、消息传输、用户交互和数据留存”五个方面展开。与传统单点采集系统不同，当前项目源码所体现的是一个由 STM32、ESP32、Qt、Web 和 SQLite 共同构成的多端协同系统，因此需求分析不能仅停留在单片机端采样，而应覆盖完整业务链路。

    ### 2.1.1 数据采集与感知层

    数据采集与感知层的首要需求，是持续获取环境类数据和资源类数据。结合真实硬件结构，系统需要采集温湿度、空气质量、PM2.5、电流和水流信息，并在现场端完成必要的单位换算、异常值抑制和资源量推导。类似的多传感器环境监测系统研究表明，嵌入式采集端若能在本地完成基础预处理，能够提升后续业务处理的一致性[8][10]。因此，本系统要求感知层既能读数，也能组织出可直接用于显示、判断和存储的工程量数据。

    ### 2.1.2 嵌入式控制与通信层

    嵌入式控制与通信层需要承担现场自治和远程互联两类任务。一方面，STM32 端需要完成阈值比较、三级状态判定、组合联动和执行器驱动；另一方面，ESP32 需要通过串口与 MQTT 机制把现场节点接入网络消息链路。MQTT 的发布/订阅机制能够降低设备间直接耦合度，适合小型监测系统中的状态分发和控制回传[5][9]。因此，该层的功能需求不仅包括“采到数据”，还包括“能判断、能联动、能上报、能下发”。

    ### 2.1.3 用户交互与展示层

    用户交互与展示层在本系统中分为 Qt 桌面端和 Web 看板两个部分。Qt 桌面端需要承担实时监控、阈值配置、远程控制、报警管理、历史查询和用户管理等管理型功能；Web 看板需要承担实时状态展示、历史趋势呈现和一键求助等轻量交互功能。基于 Qt 的远程监控系统研究表明，桌面端在多模块集成和复杂业务管理方面具有较好适配性[11]，而浏览器侧监测系统更适合承担跨终端访问和轻量展示任务[14]。

    ### 2.1.4 用户角色和权限

    当前系统源码并未实现复杂的企业级权限体系，但已经具备基本的用户角色和身份识别能力。因此，功能需求层面至少需要支持用户登录、注册、角色区分和账户信息维护。结合实际实现，系统中的角色主要包括管理员和普通用户两类，其作用重点在于身份区分和界面信息管理，而非复杂的功能隔离。

    ## 2.2 非功能性需求

    非功能性需求决定了系统在实际运行中的质量表现。对于灾后临时安置点场景而言，系统不仅要“能用”，还要在性能、安全性和可用性方面满足现场部署条件。

    ### 2.2.1 性能需求

    性能需求主要体现在采样节奏、联动响应和多端刷新效率三个方面。根据实际源码设计，系统需要在秒级周期内完成数据采集与状态更新，在可接受时延范围内完成 MQTT 分发和终端刷新，并保证本地联动优先于远程展示。这意味着系统不追求高并发大吞吐场景下的极限性能，而强调资源受限条件下的稳定响应能力。

    ### 2.2.2 安全性需求

    安全性需求在本系统中主要体现为通信入口控制、用户身份校验和本地数据边界控制。由于系统没有独立后端安全框架，因此需要通过 MQTT 接入密钥、本地用户表校验、结构化命令约束等方式降低误操作和无效消息风险。同时，系统应避免开放无约束控制入口，确保远程命令必须经过既定主题和既定消息格式才能生效。

    ### 2.2.3 可用性需求

    可用性需求主要包括网络中断后的恢复能力、上位端不在线时的本地自治能力以及界面操作的直观性。现场控制端需要在网络波动时继续运行，ESP32 和 Qt 端需要具备自动重连能力，Web 看板需要保持轻量可访问。这种设计有助于保证系统在复杂环境下仍能维持核心功能连续性。

    ## 2.3 约束条件

    本系统的约束条件主要来自硬件资源边界和技术路线选择边界。真实源码表明，系统属于轻量化场景部署方案，因此其需求分析必须体现这种边界意识。

    ### 2.3.1 硬件选型约束

    STM32F103 和 ESP32 的选用决定了系统必须在有限的处理能力、存储空间和外设资源条件下完成多源采集、状态判定和消息交互。STM32F103 数据手册表明，该系列器件适合通用嵌入式控制场景，但并不适合承载复杂操作系统或重型通信栈[15]。ESP32 具备较好的 WiFi 接入能力和串口扩展能力，但其在本系统中的定位仍是轻量网关而非复杂平台节点[16]。

    ### 2.3.2 技术选型约束

    从技术选型看，当前项目并不存在独立后端服务和集中式数据库平台，因此系统必须采用更轻量的实现路径。Qt 负责桌面管理与本地数据处理，Web 负责轻量展示，SQLite 负责本地持久化，MQTT 负责消息中转。这样的技术选择降低了部署复杂度，但同时也要求系统在同一主题双向通信、本地认证、本地存储和多端一致性方面进行额外设计约束。

    ## 2.4 数据需求

    系统的数据需求不仅包括传感器数值本身，还包括状态、事件、控制和日志等多种信息类别。只有对这些数据进行结构化组织，系统才能形成真正的管理闭环。

    ### 2.4.1 数据类型

    结合真实源码，系统需要处理的数据可分为环境数据、资源数据、状态数据、控制数据和业务记录数据五类。其中环境数据包括温湿度、空气质量和 PM2.5，资源数据包括电流、水流及其派生出的累计与剩余信息，状态数据包括全局等级、分项等级、执行器状态、模式状态和故障状态，控制数据包括设备控制、阈值设置和恢复命令，业务记录数据则包括历史采样、报警、求助、日志和用户信息。

    ### 2.4.2 数据来源

    系统数据来源具有明显的多源特征。传感器数据来自 STM32 现场采集，控制数据主要来自 Qt 桌面端下发，求助数据来自 Web 看板，用户数据来自本地认证流程，离线记录则来自 Qt 对消息更新时间的判断。不同来源的数据最终汇入统一的消息链路和本地数据库。

    ### 2.4.3 数据流向

    数据流向上，现场监测值由 STM32 生成，经 ESP32 发布到 MQTT 主题，再由 Qt 和 Web 同时消费；Qt 在消费后将关键数据写入 SQLite，并据此生成历史、报警和日志记录；Web 的求助消息则通过独立主题回到 Qt，再写入报警信息表。由此形成“现场采集—消息分发—多端消费—本地留存”的数据闭环。

    ## 2.5 接口需求

    接口需求分析的核心，是明确系统各节点之间如何交换信息，以及这些接口应当承载何种业务能力。

    ### 2.5.1 接口类型

    本系统的接口类型主要包括传感器采样接口、执行器控制接口、STM32 与 ESP32 之间的 UART 接口、ESP32 与 MQTT 服务之间的网络消息接口、Web 看板的 WebSocket 接口，以及 Qt 与 SQLite 之间的数据访问接口。不同接口共同构成了感知、控制、传输、展示和存储的整体结构。

    ### 2.5.2 协议和数据传输格式

    系统在网络侧采用 MQTT 作为消息传输协议，在设备侧采用串口文本帧方式传输，在消息负载层采用 JSON 组织环境数据、资源数据、状态数据和控制数据。MQTT 作为轻量级发布/订阅协议，适用于资源受限设备之间的解耦通信[5][6]。因此，本系统的接口设计要求消息结构清晰、字段稳定，并能够同时支持状态上报和控制下发。

    ### 2.5.3 接口功能和性能需求

    从功能上看，接口需要支持上行状态快照传输、下行控制命令传输、阈值同步、求助消息上报以及本地数据写入。从性能上看，接口应满足完整报文收发、异常情况下的重连恢复和同主题双向通信下的回环抑制需求。只有在这些条件满足的情况下，系统各层之间的协同才能稳定成立。

    ## 2.6 用户界面和交互设计需求

    本系统的界面需求不是单一终端需求，而是围绕桌面管理端与浏览器展示端进行分层设计。不同终端面向的用户和使用目标不同，因此在交互设计上也存在明显差异。

    ### 2.6.1 Qt 桌面端界面设计要求

    Qt 桌面端需要承担管理中心角色，因此界面设计要求突出模块完整性、状态可读性和操作可追踪性。具体而言，系统应能够清晰展示实时监测数据、水电状态、报警信息、历史曲线和设备信息，并提供阈值修改、远程控制、用户管理和日志查看等交互入口。界面不仅要显示数据，还要帮助工作人员区分正常、预警、告警和离线等不同状态。

    ### 2.6.2 Web 看板交互设计要求

    Web 看板主要面向轻量访问与快速浏览，因此设计要求强调大屏化、低门槛和弱操作负担。页面需要能够在浏览器中直观展示环境指标、资源状态、执行器动作和报警提示，并提供历史趋势查看和一键求助功能。其交互目标不是替代 Qt 桌面端的管理能力，而是在统一消息链路下提供更便捷的展示入口。

    ## 2.7 小结

    本章基于真实源码系统，对功能性需求、非功能性需求、约束条件、数据需求、接口需求以及界面交互需求进行了分析。分析结果表明，当前系统的需求边界集中在多源采集、三级预警、双向通信、多端展示和本地数据闭环几个方面。上述需求分析为后续总体设计和详细实现提供了明确依据。

    [[PAGEBREAK]]
    # 第 3 章 总体设计

    ## 3.1 开发环境与技术选型

    系统总体设计的首要问题，是根据真实源码所体现的功能边界选择合适的软硬件开发环境。由于本系统强调现场自治、轻量通信和多端协同，因此开发环境与技术选型必须同时兼顾嵌入式控制、桌面管理、浏览器展示和本地数据管理。

    ### 3.1.1 嵌入式开发环境选择

    STM32 部分采用 C 语言裸机开发方式实现，工程文件显示其开发环境为 Keil uVision，底层外设访问围绕 STM32F103 系列硬件资源展开。STM32F103 具备定时器、GPIO、ADC 和串口等常用资源，适合本系统所需的多传感器采集、PWM 驱动、串口通信和本地控制任务[15]。因此，采用 STM32F103 作为现场主控，能够在较低硬件成本下实现完整控制闭环。

    ### 3.1.2 Qt 客户端开发环境选择

    Qt 客户端采用 Qt 6 与 C++ 实现，实际工程中使用了 Widgets、Charts、Sql、Network 和 Mqtt 等模块。相关研究表明，Qt 适合构建具有多页面组织、实时数据显示和交互控制能力的物联网桌面监控系统[11]。对于本系统而言，Qt 不仅负责界面渲染，还承担 MQTT 接入、本地数据库访问、报警管理和远程控制等核心业务，因此其开发环境选择与系统功能定位是一致的。

    ### 3.1.3 Web 看板与数据库技术选择

    Web 看板采用 HTML、CSS 与 JavaScript 实现，并通过 MQTT.js 与 Chart.js 完成消息接入和趋势可视化。该方案具有部署轻量、访问门槛低和跨平台性好的特点，适合本系统中的浏览器侧展示需求[14]。数据库方面，系统采用 SQLite 本地文件数据库，用于保存历史数据、报警信息、离线记录、日志和用户信息，从而避免了独立数据库服务器带来的部署复杂度。

    ## 3.2 系统硬件架构

    系统硬件架构围绕 STM32 主控、传感器与执行器模块、ESP32 通信网关三部分展开，其设计目标是形成“现场采集—本地控制—网络接入”的硬件闭环。

    ### 3.2.1 STM32 主控架构

    STM32F103 是现场端的核心控制器，负责协调传感器采样、状态判定、执行器联动、OLED 显示和串口数据组织。整个硬件架构中，STM32 既不是单纯采集节点，也不是简单串口透传节点，而是本地业务逻辑的唯一执行中心。所有上报数据中的状态、级别和动作结果，均以 STM32 的本地处理结果为准。

    ### 3.2.2 传感器与执行器架构

    传感器侧包括 DHT11、MQ-135、GP2Y10、ACS712 和水流传感器，分别对应温湿度、空气质量、颗粒物、电流和水流信息采集。执行器侧包括风扇、舵机、蜂鸣器、三色指示灯和 OLED 显示模块，用于完成现场状态提示和联动控制。该架构体现出“多源感知 + 多方式反馈”的设计特点，使系统既能采集，又能在现场直接响应。

    ### 3.2.3 ESP32 通信网关架构

    ESP32 位于硬件架构中的网络接入层，负责把 STM32 的串口消息接入 WiFi 和 MQTT 通信链路。其硬件能力适合承担轻量网关任务，能够在保持实现简单的前提下完成无线接入、消息收发和重连恢复[16]。因此，ESP32 在本系统中的定位并非第二控制器，而是桥接 STM32 与上位端的通信网关。

    ## 3.3 系统软件总体设计

    软件总体设计的核心，是围绕真实系统模块划分职责，并保证各模块之间通过统一消息模型协同工作。

    ### 3.3.1 系统五层架构设计

    结合源码实现，可以将系统划分为感知采集层、嵌入控制层、网络接入层、数据管理层和交互展示层五个层次。感知采集层负责环境与资源信息获取；嵌入控制层负责 STM32 本地状态判断与控制；网络接入层负责 ESP32 的消息桥接；数据管理层由 Qt 与 SQLite 共同构成；交互展示层由 Qt 桌面端和 Web 看板组成。这样的五层结构有助于明确各模块职责边界。

    ### 3.3.2 各子系统功能划分

    在该结构下，STM32 子系统负责传感器采集、阈值判断、三级联动和 JSON 组织；ESP32 子系统负责 WiFi 接入、MQTT 会话与串口桥接；Qt 子系统负责实时监控、阈值设置、远程控制、报警管理、数据落库与用户管理；Web 子系统负责实时展示、历史趋势和一键求助；SQLite 子系统负责业务数据持久化。各子系统之间通过消息和本地数据库协同，而不是通过独立后端服务耦合。

    ### 3.3.3 系统协同工作流程

    系统协同工作流程表现为：STM32 周期采集数据并完成状态判定，经串口发送给 ESP32；ESP32 缓存后发布到 MQTT 主业务主题；Qt 与 Web 同时订阅该主题进行界面刷新；Qt 进一步把有效数据写入 SQLite，并依据报警变化维护事件记录；当 Qt 或 Web 产生控制与求助消息时，再分别通过既定主题返回到系统内部，形成完整闭环。

    ## 3.4 嵌入式控制与通信总体设计

    嵌入式控制与通信总体设计直接决定了系统能否在资源受限条件下同时完成现场自治与远程互联。

    ### 3.4.1 数据采集与预处理设计

    数据采集与预处理设计遵循“先本地归一化，再统一上报”的原则。STM32 在读取各传感器原始量后，会完成工程量换算、异常值过滤以及资源类派生量计算，使上位端接收到的不是未经处理的原始值，而是可直接用于展示和判断的数据结果。这样能够减少多端重复计算带来的不一致问题。

    ### 3.4.2 串口通信总体设计

    STM32 与 ESP32 之间的通信采用 UART 文本帧方式组织。总体设计上，串口不仅承担普通调试输出，更承担上行状态上报和下行控制接收双重职能。因此，串口帧必须具备明确边界，并能够同时兼容状态 JSON、结构化命令和简化控制指令。

    ### 3.4.3 MQTT 消息中转设计

    系统在网络侧采用 MQTT 完成消息中转，主业务主题同时承担状态上报和控制下发任务。MQTT 的发布/订阅机制能够使现场节点、Qt 桌面端和 Web 看板围绕同一主题进行解耦交互[5][9]。为保证链路稳定，ESP32 在总体设计上采用“串口接收缓存 + 网络定时发布 + 下行分类转发”的策略，从而在实时性与稳定性之间取得平衡。

    ### 3.4.4 自动联动与告警总体设计

    自动联动与告警设计以三级状态机为基础，通过单项判定和组合判定共同生成系统级风险结果。相关生态环境监测系统研究表明，监测节点若能结合多类指标进行综合判断，更有利于提升异常识别的完整性和系统响应能力[17]。因此，本系统在总体设计中既保留单一指标的预警与告警机制，也对多指标同时异常的情形设置组合联动规则，使风扇、舵机、蜂鸣器和指示灯能够体现差异化动作。

    ## 3.5 Qt 桌面端总体设计

    Qt 桌面端在本系统中承担管理中心角色，其总体设计目标是把实时监控、配置控制和数据管理整合到统一界面中。

    ### 3.5.1 实时监测模块设计

    实时监测模块负责展示环境类与资源类指标、设备总体状态、趋势曲线和设备状态表。相关监测软件研究表明，桌面端若能够把实时值、等级状态和趋势变化同时组织到一个界面中，更有利于用户快速理解当前运行情况[19]。因此，本系统在 Qt 端把实时大字显示、等级颜色、曲线更新和离线提示共同纳入统一监测模块。

    ### 3.5.2 阈值配置与远程控制模块设计

    阈值配置与远程控制模块用于完成告警参数调整、默认阈值恢复以及执行器远程干预。该模块的设计重点是把控制请求结构化，并保证控制动作与日志记录同步发生。这样，桌面端不只是显示终端，同时也是远程管理终端。

    ### 3.5.3 报警与求助处理模块设计

    报警与求助处理模块负责接收现场报警信息和浏览器求助信息，并将两者统一纳入事件管理流程。相关平台日志分析与管理系统研究表明，结构化记录、状态追踪和日志闭环是提高系统可追溯能力的重要基础[18]。因此，本系统在 Qt 端将报警和求助信息统一写入事件表，并通过状态字段描述其发生、持续和结束过程。

    ## 3.6 Web 看板总体设计

    Web 看板总体设计强调轻量、直观和跨终端访问能力，其作用不是替代桌面端，而是作为系统的浏览器侧展示入口。

    ### 3.6.1 实时数据展示设计

    实时数据展示设计围绕环境指标、资源状态、执行器状态和报警信息四类内容展开。页面通过订阅 MQTT 主业务主题，直接消费现场控制端上报的统一报文，并把等级、动作和资源信息同步映射到卡片、状态条和提示区，从而保证与 Qt 桌面端的数据来源一致。

    ### 3.6.2 历史趋势展示设计

    历史趋势展示设计并不直接访问数据库，而是通过导出脚本生成静态数据文件，再由浏览器用图表方式呈现。这种设计既降低了 Web 端实现复杂度，也使历史数据访问保持在可控边界内。对于小规模监测系统而言，这是一种更适合轻量部署的方案[13]。

    ### 3.6.3 一键求助交互设计

    一键求助交互设计是 Web 看板区别于纯展示页面的重要部分。该设计允许现场用户通过简单操作把求助事件提交到系统内部，再由 Qt 客户端统一处理。这样既保留了浏览器端的低门槛访问优势，也使求助事件纳入同一消息与数据库闭环中。

    ## 3.7 数据库总体设计

    数据库总体设计的目标，是在无独立后端条件下支撑历史记录、事件追踪、日志审计和用户管理等业务需求。

    ### 3.7.1 数据库结构设计

    系统采用 SQLite 单库多表结构，主要表包括 `users`、`data`、`alarm_info`、`device_abnormal_records`、`device_info` 和 `remote_exec_logs`。各表围绕用户认证、历史采样、报警事件、离线记录、设备信息和远程控制日志进行划分，既满足当前业务边界，又保持结构简洁。

    ### 3.7.2 数据存储逻辑设计

    数据存储逻辑遵循“有效消息即入库、事件变化即记录、控制发起即留痕”的原则。实时监测值进入 `data` 表，报警与求助进入 `alarm_info` 表，设备离线状态进入 `device_abnormal_records` 表，远程控制动作进入 `remote_exec_logs` 表，用户信息进入 `users` 表。这样的逻辑使数据库真正承担起本地数据中心作用。

    ### 3.7.3 数据查询与导出设计

    数据查询与导出设计主要服务于历史显示和后续分析。Qt 客户端可直接查询本地数据库完成历史页面刷新和报警记录展示；Web 看板则通过 Python 脚本从 `data` 表聚合生成 `history_data.js` 文件，实现浏览器端的趋势展示。该设计体现了“统一存储、多端复用”的总体思路。

    ## 3.8 小结

    本章从开发环境、硬件架构、软件架构、通信设计、桌面端设计、Web 看板设计和数据库设计等方面，对系统总体方案进行了阐述。总体来看，系统采用的是一种围绕 STM32 现场自治、ESP32 网络接入、MQTT 消息分发、Qt 本地管理和 Web 轻量展示展开的多端协同结构，这为后续详细实现奠定了基础。

    [[PAGEBREAK]]
    # 第 4 章 详细设计与实现

    ## 4.1 硬件详细设计与实现

    硬件详细设计部分主要围绕传感器采集模块、执行器控制模块和本地显示模块展开，其目标是说明系统如何通过实际硬件连接完成环境监测、水电感知与现场响应。

    ### 4.1.1 温湿度采集模块设计

    温湿度采集模块采用 DHT11 传感器接入 STM32，用于周期获取温度和湿度信息。该模块实现方式较为直接，但在系统中承担基础环境感知角色，是后续三级状态判断和环境舒适度分析的重要输入。类似 STM32 环境监测系统研究中，温湿度模块通常被作为基础感知单元接入主控[8]，本系统的实现思路与此一致，但进一步将其纳入统一 JSON 报文与联动判断链路。

    ### 4.1.2 PM2.5 与空气质量采集模块设计

    PM2.5 与空气质量采集模块由 GP2Y10 和 MQ-135 组成，分别负责颗粒物浓度与空气质量指标获取。系统在 STM32 本地完成采样值换算与状态判定，而不是把原始电压值直接上传。这一设计使 PM2.5 和空气质量信息能够直接进入环境预警和组合联动逻辑，从而提高了业务数据可用性。

    ### 4.1.3 电流与水流采集模块设计

    电流与水流采集模块由 ACS712 与水流传感器构成，分别反映临时用电和供水状态。实现上，系统不仅获取瞬时值，还进一步推导累计用量、剩余比例和估计可用时间等资源信息，使其既能支持现场状态判断，也能支持上位端的水电管理展示。该部分体现了系统由“环境监测”向“环境监测 + 资源管理”的扩展。

    ### 4.1.4 执行器与本地显示模块设计

    执行器与本地显示模块包括风扇、舵机、蜂鸣器、三色指示灯和 OLED。风扇和舵机承担自动联动动作，蜂鸣器与指示灯承担现场提示功能，OLED 则承担本地状态显示功能。这样设计的作用在于，即使上位端暂时离线，现场节点仍可以通过本地显示和本地动作表达系统状态。

    ## 4.2 STM32 控制程序详细设计与实现

    STM32 控制程序是整个系统的核心，其实现内容既包括现场数据采集，也包括状态机式预警控制和对外通信组织。

    ### 4.2.1 主程序架构设计

    主程序采用裸机循环结构，以固定短节拍为基础，在同一主循环中分层安排串口命令处理、周期采样、联动控制和 OLED 刷新。其关键实现特点是不用操作系统，而通过统一节拍完成多周期任务分发。这样既降低了实现复杂度，也保证了现场控制逻辑的确定性。

    ### 4.2.2 数据采集与预处理实现

    在数据采集与预处理实现中，STM32 端周期读取 DHT11、MQ-135、GP2Y10、ACS712 和水流传感器数据，并完成工程量换算、异常值过滤和资源量推导。其关键逻辑不是“读完即发”，而是“先处理再发”，这样可以保证上位端接收到的数据已经满足展示与比较需求。

    ### 4.2.3 阈值判定与自动联动实现

    阈值判定与自动联动实现采用“告警阈值 + 预警阈值 + 连续确认 + 滞回恢复”的状态机策略，并在此基础上叠加组合异常判定。分级响应与联动控制在应急系统中具有明显价值，能够提升异常处置的针对性[17]。本系统中，风扇、舵机、蜂鸣器和指示灯的动作均与这种分级机制直接关联，从而形成现场闭环。

    ### 4.2.4 JSON 上报与命令解析实现

    系统把传感器数据、资源信息、等级结果、执行器状态、模式状态、报警信息和故障信息统一组织为 JSON，通过串口发送给 ESP32。与此同时，下行命令解析支持设备控制、阈值设置、模式切换和恢复默认等多类消息。这样设计的意义在于，使 STM32 既能对外稳定输出完整状态快照，又能对内保持统一的命令入口。

    ## 4.3 ESP32 通信网关详细设计与实现

    ESP32 详细实现部分重点说明其作为网关的桥接作用，而不是二次控制作用。

    ### 4.3.1 WiFi 连接与重连实现

    ESP32 上电后首先连接预设 WiFi 网络，在运行过程中若发现网络中断，则优先恢复 WiFi，再继续重建 MQTT 会话。这样的实现策略把网络恢复放在前、消息恢复放在后，有助于避免无效重试。其作用是保证网关能够在无线网络波动条件下持续服务于系统链路。

    ### 4.3.2 MQTT 订阅与发布实现

    在 MQTT 订阅与发布实现中，ESP32 与 `bemfa.com:9501` 建立会话，并围绕 `test001up` 主题完成上行发布与下行订阅。MQTT 的轻量特性和主题解耦能力，使其适合承担现场采集节点与多客户端之间的消息中转[5]。本系统进一步通过较大的缓冲区和定时发布策略，保证完整 JSON 报文能够稳定转发。

    ### 4.3.3 串口与 MQTT 桥接实现

    串口与 MQTT 桥接实现采用“串口分帧缓存—保留原始 JSON—按更新触发发布”的方式。ESP32 不对 STM32 的业务数据进行重算，而是尽量保持报文原貌上传到 MQTT。由此，Qt、Web 与串口原始数据之间能够保持结构一致，这也是多端显示一致性的前提。

    ### 4.3.4 阈值同步与消息过滤实现

    为适应同主题双向通信，本系统在 ESP32 端加入了消息过滤与阈值同步机制。对于包含状态上报特征的消息，网关直接丢弃其下行处理路径；对于阈值同步消息，则通过特定标识避免往返循环。该实现有效解决了同主题上行与下行并存时的回环问题。

    ## 4.4 Qt 桌面端详细设计与实现

    Qt 桌面端是系统的业务管理中心，其详细实现围绕消息接收、界面刷新、控制下发、报警处理和用户管理展开。

    ### 4.4.1 MQTT 数据接收与解析实现

    Qt 客户端通过 MQTT 接收 `test001up` 与 `WebQT1` 两类消息，并优先按当前系统的结构化 JSON 格式进行解析。基于 Qt 的远程监控系统研究表明，桌面端把消息解析、界面刷新与控制交互整合到统一程序中，能够较好支持管理型应用场景[11]。本系统中，Qt 直接消费 STM32 判定结果，而不是自行重新计算各项等级，从而保证了状态来源统一。

    ### 4.4.2 实时数据展示模块实现

    实时数据展示模块在收到有效消息后，同步更新温湿度、空气质量、PM2.5、水流、电流等监测值，以及总体运行状态、设备状态表和趋势图。相关监测系统研究表明，实时值、状态等级和趋势曲线的组合展示更有利于用户快速理解当前运行情况[19]。本系统中，Qt 端通过颜色、文字和曲线三种方式共同表达状态，增强了界面的可判读性。

    ### 4.4.3 远程控制与阈值下发模块实现

    远程控制与阈值下发模块支持设备控制、告警阈值设置和默认阈值恢复三类操作。其核心实现思想是：桌面端只负责表达控制意图与参数配置请求，而具体的告警边界重建和执行器动作仍由 STM32 在现场完成。这样既保留了远程干预能力，又避免了上位端与现场端在判定逻辑上的分叉。

    ### 4.4.4 报警与求助处理模块实现

    报警与求助处理模块通过比较前后两次报警集合变化，生成报警开始记录和结束记录，同时把 Web 求助信息统一写入报警信息表。面向突发事件的信息化管理研究指出，把事件从“瞬时提示”转化为“生命周期对象”，有助于提高追踪与处置能力[18]。本系统正是通过这一实现方式，把报警与求助纳入同一管理流程。

    ### 4.4.5 用户登录与管理模块实现

    用户登录与管理模块基于 SQLite 本地 `users` 表实现，支持默认账户初始化、登录、注册、角色识别和账户信息修改。该模块的实现并不追求复杂权限体系，而是围绕“本地部署条件下的基本身份识别”展开，以满足桌面管理软件的基本使用要求。

    ## 4.5 Web 看板详细设计与实现

    Web 看板详细实现重点体现其作为轻量展示终端的设计特点。

    ### 4.5.1 MQTT WebSocket 接入实现

    Web 看板通过 MQTT.js 以 WebSocket 方式接入消息链路，订阅主业务主题以接收实时数据。类似浏览器侧监测系统的实现经验表明，浏览器直连消息总线能够显著降低 Web 侧部署负担[14]。因此，本系统没有再额外设计 Web 后端，而是让浏览器直接参与消息消费。

    ### 4.5.2 实时数据展示模块实现

    实时数据展示模块把环境指标、资源状态、执行器动作和报警提示划分到不同区域，并依据统一 JSON 报文完成同步刷新。其核心实现逻辑不是重新判断状态，而是直接使用来自现场端的等级和动作结果，从而保证与 Qt 桌面端在状态来源上保持一致。

    ### 4.5.3 历史趋势展示模块实现

    历史趋势展示模块通过 Chart.js 读取导出的 `history_data.js` 文件，展示近阶段水电资源变化趋势。由于 Web 端不直接访问 SQLite，因此历史展示依赖桌面端导出脚本提供聚合结果。这种方案虽然不属于实时数据库查询，但能够在较低复杂度下满足浏览器端趋势展示需求。

    ### 4.5.4 一键求助模块实现

    一键求助模块通过浮动式按钮提供快速事件上报入口，用户选择求助类型后，浏览器将求助消息发布到 `WebQT1` 主题。该实现使 Web 看板不仅是展示界面，也成为轻量交互终端，同时又避免了浏览器端直接写数据库所带来的额外耦合。

    ## 4.6 数据库详细设计与实现

    数据库详细设计与实现部分说明各张数据表在本系统中的具体职责与运行方式。当前系统全部业务数据均保存在 SQLite 本地数据库中。

    ### 4.6.1 用户信息表（users）

    `users` 表用于保存用户名、口令、角色和创建时间等信息，支撑本地登录、注册和角色识别流程。该表在系统中承担基础认证功能，使桌面端在无独立认证服务器的条件下仍具备账号区分能力。

    ### 4.6.2 传感器数据表（data）

    `data` 表用于保存按时间组织的历史监测样本，是整个系统历史分析的原始数据源。Qt 每接收到一条有效实时消息，就把当前环境与资源监测值写入该表，从而形成连续的时序数据积累。

    ### 4.6.3 报警信息表（alarm_info）

    `alarm_info` 表用于记录传感器预警、传感器告警以及 Web 一键求助事件。该表不仅保存事件内容，还保存开始时间、结束时间、等级、处理状态和报警码，使报警信息能够以生命周期方式进行管理。

    ### 4.6.4 设备离线记录表（device_abnormal_records）

    `device_abnormal_records` 表用于记录设备或监测项在通信中断状态下的离线起点与持续时间。系统通过更新同一离线事件的持续秒数，而不是不断新建记录，来表达离线过程的连续性。

    ### 4.6.5 设备信息表（device_info）

    `device_info` 表保存设备编号、名称、类型和位置等基础信息，主要服务于 Qt 客户端的设备管理页面。虽然当前系统设备数量不大，但该表使设备展示与业务数据之间形成了更清晰的结构分离。

    ### 4.6.6 远程执行日志表（remote_exec_logs）

    `remote_exec_logs` 表用于保存远程控制操作的时间、对象、命令文本和结果描述。只要系统认定某次操作已经发起，就立即将其写入该表，从而为后续的操作追踪、问题排查和审计留痕提供依据。

    ## 4.7 小结

    本章依据真实源码，对硬件模块、STM32 控制程序、ESP32 通信网关、Qt 桌面端、Web 看板以及 SQLite 数据库的详细实现进行了说明。可以看出，系统的实现重点并不在单一模块功能堆叠，而在于通过统一消息模型把现场控制、网络转发、多端展示和本地存储组织成完整闭环。

    [[PAGEBREAK]]
    # 第 5 章 测 试

    ## 5.1 测试理论方法介绍

    本系统测试遵循“分层验证、链路验证与综合验证相结合”的思路，重点考察感知采集、消息传输、联动控制、多端一致性和本地持久化等方面。对于综合监测预警系统而言，测试不能只验证单点功能是否可用，还应覆盖感知、传输、预警和记录等关键链路[20]。因此，本章测试既包含硬件与采集测试，也包含通信链路、桌面端、Web 端和数据库协同测试。

    ## 5.2 硬件与采集测试

    硬件与采集测试的目标，是验证各传感器与执行器模块是否能够稳定工作，并确认 STM32 本地采集与预处理结果是否满足后续联动和上报要求。

    ### 5.2.1 测试环境

    测试环境由 STM32F103 主控板、DHT11、MQ-135、GP2Y10、ACS712、水流传感器、风扇、舵机、蜂鸣器、三色指示灯和 OLED 模块组成。测试时 STM32 独立完成采样、状态判断和本地显示，ESP32 与上位端保持连接但不参与硬件本体验证，以便优先观察现场节点运行状态。

    ### 5.2.2 测试方法与策略

    测试方法采用“单模块验证 + 联合运行观察”的方式。先分别验证温湿度、颗粒物、空气质量、电流和水流等采样结果是否能够稳定更新，再检查蜂鸣器、风扇、舵机和指示灯在不同状态下的动作是否符合预期，最后观察各模块在持续运行条件下的协同表现。

    ### 5.2.3 测试用例与结果

    测试结果表明，系统能够稳定获取温湿度、空气质量、PM2.5、电流和水流等关键数据，并可在 OLED 与上位端同步显示。对电流等容易受波动影响的数据进行观察时，未出现持续性异常跳变，说明本地预处理和过滤策略有效。执行器方面，预警与告警触发后相应动作能够及时出现，硬件链路满足系统运行要求。

    ### 5.2.4 测试结论

    硬件与采集测试说明，当前系统具备稳定的现场感知和本地响应能力。STM32 端不仅能够完成多传感器采样，还能够把处理后的结果直接用于本地提示和后续通信，为整套系统的上层协同提供了可靠基础。

    ## 5.3 通信链路测试

    通信链路测试用于验证系统“STM32—ESP32—MQTT—Qt/Web”的数据上行链路和“Qt—MQTT—ESP32—STM32”的控制下行链路是否稳定有效。

    ### 5.3.1 STM32 与 ESP32 串口通信测试

    串口通信测试主要观察 STM32 输出的 JSON 报文能否被 ESP32 正确接收并缓存，以及下行命令能否按帧返回到 STM32。测试现象表明，ESP32 能够稳定接收完整串口文本帧，STM32 也能够根据下行命令执行对应动作，说明设备侧桥接链路完整可用。

    ### 5.3.2 ESP32 与 MQTT 平台通信测试

    MQTT 平台通信测试重点检查 ESP32 的 WiFi 接入、MQTT 连接、主题订阅、上行发布和断线重连情况。测试结果表明，在网络正常条件下，ESP32 可持续向主业务主题发布状态消息；在网络短时中断后，系统能够先恢复无线连接，再恢复 MQTT 会话，说明网关具备基本的自恢复能力。

    ### 5.3.3 Qt 与 Web 消息交互测试

    Qt 与 Web 消息交互测试主要验证两个客户端订阅同一主题后是否能够保持状态一致，以及 Web 求助消息能否被 Qt 正确接收并处理。测试现象表明，Qt 与 Web 对环境数据、资源状态和报警信息的显示整体保持同步；Web 发起的一键求助可被 Qt 客户端接收并写入报警记录，说明多端协同链路有效。

    ### 5.3.4 通信测试结论

    通信测试说明，当前系统已形成较完整的双向消息链路。STM32 与 ESP32 之间的串口传输、ESP32 与 MQTT 平台之间的消息分发，以及 Qt 与 Web 的主题订阅协同均能够稳定运行，验证了系统总体通信架构的有效性。

    ## 5.4 Qt 客户端与数据库测试

    Qt 客户端与数据库测试主要考察桌面端在实时监测、报警管理、远程控制和本地持久化方面的综合表现。

    ### 5.4.1 测试环境

    测试环境包括运行于 Windows 的 Qt 客户端、MQTT 消息服务、ESP32 网关节点以及本地 SQLite 数据库。测试过程中重点观察客户端连接状态、实时页面刷新、控制操作执行以及数据库各表记录变化情况。

    ### 5.4.2 测试方法与策略

    测试方法采用“实时消息接收—界面刷新—数据落库—事件回查”的闭环策略。首先检查客户端是否能够持续接收 MQTT 数据并刷新界面；其次验证远程控制和阈值下发是否能够形成控制请求与日志记录；最后检查 `data`、`alarm_info`、`device_abnormal_records` 和 `remote_exec_logs` 等数据表是否正确变化。

    ### 5.4.3 测试用例与结果

    测试结果表明，Qt 客户端能够稳定接收并解析现场消息，实时页面和趋势图刷新正常；控制下发后，现场状态能够回传到界面，远程操作日志同步生成；报警发生和解除时，报警信息表能够正确记录事件开始与结束状态；在模拟离线场景下，数据库中能够生成离线持续记录。说明 Qt 客户端与 SQLite 已形成稳定的数据闭环。

    ### 5.4.4 测试结论

    该部分测试验证了 Qt 客户端既是管理界面，也是本地业务中心。它不仅能显示数据，还能完成控制、报警、存储和追踪等核心管理任务，满足系统作为桌面管理端的设计目标。

    ## 5.5 Web 看板功能测试

    Web 看板功能测试主要关注浏览器端的实时展示能力、历史趋势呈现能力以及一键求助交互是否满足轻量使用需求。

    ### 5.5.1 测试环境

    测试环境包括浏览器、Web 看板页面、MQTT WebSocket 通道以及由 SQLite 导出的历史趋势数据文件。测试中同时保持 Qt 客户端在线，以观察 Web 与 Qt 在同一消息链路下的协同表现。

    ### 5.5.2 测试方法与策略

    测试方法以“实时消息显示—状态同步观察—历史图表检查—求助消息发送”为主。具体包括观察指标卡片和资源卡片是否随 MQTT 消息同步更新，查看历史趋势图是否能够正确加载导出数据，以及验证求助按钮是否能够生成有效消息。

    ### 5.5.3 测试用例与结果

    测试结果表明，Web 看板能够稳定显示温湿度、空气质量、PM2.5、水流和电流等实时数据，并根据系统等级变化更新提示状态；历史趋势图能够读取导出数据并正确绘制曲线；一键求助消息发出后，Qt 客户端能够同步接收并处理。说明 Web 看板已达到“轻量展示 + 简易交互”的设计目标。

    ### 5.5.4 测试结论

    Web 看板功能测试说明，浏览器端实现方式在不引入独立 Web 后端的条件下，仍能够完成实时展示、趋势查看和事件上报任务，适合本系统面向展示和辅助交互的定位。

    ## 5.6 测试总结与建议

    在完成分层测试后，还需要从整体角度对系统表现进行归纳，并指出现阶段仍存在的提升空间。

    ### 5.6.1 测试总结

    综合测试结果表明，系统已经完成从现场采集、状态判断、消息传输、多端展示到本地存储的完整业务闭环。硬件链路、通信链路、桌面端和 Web 端均能够围绕统一数据模型协同运行，说明系统总体设计满足当前课题目标。

    ### 5.6.2 存在问题

    尽管当前系统能够稳定运行，但仍存在一些可继续完善的地方。例如，主业务通信仍采用同主题双向复用方式，后续可进一步细化主题设计；系统虽完成了基本监测与预警，但长期现场环境下的更高强度连续运行能力仍需更充分验证。综合监测预警网络应用研究同样指出，系统在从原型验证走向长期应用时，持续稳定性与协同扩展性是后续重点问题[20]。

    ### 5.6.3 改进建议

    后续改进可从三个方向展开：一是增强长期部署条件下的可靠性验证与硬件保护设计；二是进一步细化报警处理、用户角色与日志审计机制；三是完善多端协同和数据共享方式，使系统更贴近完整的智慧应急应用场景。相关可视化监测系统研究表明，前端交互能力与数据协同方式的持续优化，对于提升系统应用体验和扩展能力具有重要作用[21]，这一点也为本系统后续扩展提供了方向。

    ## 5.7 小结

    本章从测试理论方法、硬件与采集、通信链路、Qt 客户端与数据库、Web 看板功能以及测试总结等方面，对系统进行了较为完整的验证。测试结果说明，当前系统能够满足毕业设计所要求的功能正确性、实时性、稳定性、多端一致性和数据可靠性要求。
    """
).strip()


CONCLUSION_MD = dedent(
    """
    结  论

    本文面向灾后临时安置点环境监测与水电管理场景，结合真实项目源码，设计并实现了一套基于 STM32、ESP32、MQTT、Qt、Web 和 SQLite 的轻量化多端协同系统。系统围绕“现场采集、状态判定、消息传输、多端展示、本地存储”五个环节建立了完整闭环，并在实际实现中形成了较清晰的模块分工与数据流向。

    通过本课题研究，主要完成了以下工作。第一，完成了以 STM32F103 为核心的现场控制节点设计，实现了温湿度、空气质量、PM2.5、电流和水流等多源数据采集，并在本地完成三级预警、组合联动和执行器控制。第二，完成了基于 ESP32 和 MQTT 的双向通信链路设计，使现场状态能够稳定上传，同时支持 Qt 管理端进行远程控制和阈值下发。第三，完成了 Qt 桌面客户端和 Web 看板的多端实现，使系统同时具备管理型交互能力和轻量展示能力。第四，构建了基于 SQLite 的本地数据闭环，实现了历史采样、报警事件、求助信息、离线记录和远程操作日志的持久化管理。第五，通过功能测试、通信链路测试和数据可靠性测试，验证了系统总体设计和实现方案的可行性。

    从实现效果看，本文系统具有以下特点。其一，系统采用统一 JSON 数据模型连接 STM32、ESP32、Qt 与 Web，多端状态来源一致，降低了信息分叉风险。其二，系统把关键控制逻辑放在 STM32 本地完成，使其在上位端断连条件下仍具备基本自治能力。其三，系统未依赖独立后端服务，而是通过 Qt 与 SQLite 完成本地业务管理与数据留存，较好地契合了小规模、快速部署场景的实际需求。

    当然，本文工作仍存在一定局限。当前系统主要面向单站点、小规模节点场景设计，在更复杂网络环境和更长周期现场运行条件下的持续可靠性仍需进一步验证；系统当前采用同主题双向通信方式，虽然通过网关过滤机制解决了回环问题，但在后续扩展中仍可进一步细化主题组织；用户认证与权限管理目前仍较为轻量，若面向更正式应用场景，还需进一步增强安全机制与角色控制能力。

    总体而言，本文基于真实源码实现完成了一套面向灾后临时安置点的智慧水电管理与环境监测系统，较好地实现了从现场感知、状态预警、消息传输、多端展示到数据追溯的完整链路，达到本科毕业设计的预期目标，并为后续面向智慧应急场景的小型物联网系统扩展提供了实践基础。
    """
).strip()


THANKS_MD = dedent(
    """
    致  谢

    本文的完成离不开许多老师、同学和家人的支持与帮助。首先，衷心感谢指导教师在课题选题、系统设计、论文撰写和修改过程中给予的耐心指导。无论是在技术路线把握上，还是在论文结构与表达规范上，老师都提出了许多有针对性的意见，使我能够逐步完善系统实现和论文内容。

    同时，感谢在毕业设计期间给予我帮助的同学和朋友。在系统实现、测试验证和资料整理过程中，大家的交流与建议使我受益良多，也让我能够从不同角度重新审视自己的设计与实现过程。

    还要感谢家人在整个学习与论文写作期间给予的理解、支持和鼓励。正是因为有他们在生活和精神上的支持，我才能够较为专注地完成本次毕业设计任务。

    最后，感谢学校和学院为毕业设计提供的学习环境与实践条件，使我能够将所学知识与实际系统实现结合起来，完成本课题的研究与论文写作。
    """
).strip()


APPENDIX_A_MD = dedent(
    """
    附录A  数据库表结构

    ## A.1 users 用户信息表

    系统用户信息采用 `users` 表管理，主要用于桌面端登录、注册和角色识别。表中保存用户名、口令、角色与创建时间等基础信息，能够满足本系统本地认证需求。

    ```sql
    CREATE TABLE IF NOT EXISTS users (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      username TEXT NOT NULL UNIQUE,
      password TEXT NOT NULL,
      role TEXT NOT NULL DEFAULT 'user',
      created_at TEXT NOT NULL
    );
    ```

    ## A.2 data 传感器数据表

    `data` 表用于保存实时采样历史记录，是历史查询、趋势统计和导出脚本的原始数据来源。表中保存时间戳、温度、湿度、PM2.5、空气质量、电流和水流等关键数据。

    ```sql
    CREATE TABLE IF NOT EXISTS data (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      ts TEXT NOT NULL,
      temperature REAL,
      humidity REAL,
      pm25 REAL,
      air_index REAL,
      current_a REAL,
      flow_l_min REAL
    );
    CREATE INDEX IF NOT EXISTS idx_data_ts ON data(ts);
    ```

    ## A.3 alarm_info 报警信息表

    `alarm_info` 表用于统一记录传感器预警、传感器告警以及 Web 一键求助事件。除基础时间和内容字段外，系统还扩展了事件结束时间、等级、状态和报警码等字段，用于描述事件生命周期。

    ```sql
    CREATE TABLE IF NOT EXISTS alarm_info (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      alarm_time TEXT NOT NULL,
      sensor_name TEXT NOT NULL,
      alarm_content TEXT NOT NULL,
      end_time TEXT,
      level TEXT DEFAULT '预警',
      status TEXT DEFAULT 'active',
      alarm_code INTEGER DEFAULT 0
    );
    CREATE INDEX IF NOT EXISTS idx_alarm_info_time ON alarm_info(alarm_time);
    ```

    ## A.4 device_abnormal_records 设备离线记录表

    `device_abnormal_records` 表用于记录监测项离线起点与离线持续时间。系统在同一离线事件内会持续更新时长，而不是重复创建新记录。

    ```sql
    CREATE TABLE IF NOT EXISTS device_abnormal_records (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      sensor_name TEXT NOT NULL,
      offline_time TEXT NOT NULL,
      offline_duration_sec INTEGER NOT NULL DEFAULT 0,
      UNIQUE(sensor_name, offline_time)
    );
    CREATE INDEX IF NOT EXISTS idx_device_abnormal_sensor_time
    ON device_abnormal_records(sensor_name, offline_time);
    ```

    ## A.5 device_info 设备信息表

    `device_info` 表用于保存设备编号、设备名称、设备类型和安装位置等基础信息，主要服务于桌面端设备管理页面。

    ```sql
    CREATE TABLE IF NOT EXISTS device_info (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      device_id TEXT NOT NULL UNIQUE,
      device_name TEXT NOT NULL,
      device_type TEXT NOT NULL,
      location TEXT
    );
    ```

    ## A.6 remote_exec_logs 远程执行日志表

    `remote_exec_logs` 表用于记录远程控制日志，包括执行时间、设备编号、命令文本和结果描述等信息，可用于后续审计与问题追踪。

    ```sql
    CREATE TABLE IF NOT EXISTS remote_exec_logs (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      execute_time TEXT NOT NULL,
      device_id TEXT NOT NULL,
      command_text TEXT NOT NULL,
      result_text TEXT
    );
    CREATE INDEX IF NOT EXISTS idx_remote_exec_time ON remote_exec_logs(execute_time);
    ```
    """
).strip()


APPENDIX_B_MD = dedent(
    """
    附录B  通信协议与数据帧格式

    ## B.1 上行完整 JSON 报文说明

    系统上行报文由 STM32 统一组织后经串口发送至 ESP32，再由 ESP32 发布到 MQTT 主业务主题。报文以 JSON 形式组织，核心字段包括感知数据区、资源数据区、等级数据区、执行器状态区、模式状态区、报警区、动作标志区和故障区。

    ```json
    {
      "sen": {"t": 26, "h": 60, "pm": 35, "aq": 52, "f": 1.23, "i": 450},
      "res": {"bp": 85, "wp": 72, "tu": 1500, "wu": 20, "br": 8500, "wr": 80, "ps": 0, "bc": 10000, "tc": 1000, "bt": 5760, "wt": 1440},
      "lv": {"link": 0, "env": 0, "d": [0, 0, 0, 0, 0, 0]},
      "act": {"bz": 0, "fan": 0, "svo": 0, "led": [0, 1, 0]},
      "mod": {"gl": 0, "bz": 0, "fan": 0, "svo": 0, "led": 0},
      "alm": [],
      "actn": 0,
      "flt": []
    }
    ```

    ## B.2 上行字段含义

    - `sen`：传感器数据区，包含温度、湿度、PM2.5、空气质量、水流和电流信息。
    - `res`：资源状态区，包含电量、水量、累计使用量、剩余量和预计剩余时间等信息。
    - `lv`：等级状态区，包含系统整体等级、环境等级以及六类监测项的分项等级。
    - `act`：执行器当前状态区，反映蜂鸣器、风扇、舵机和指示灯当前状态。
    - `mod`：模式状态区，用于说明全局模式及执行器手动模式状态。
    - `alm`：报警码数组，用于描述当前活跃报警事件。
    - `actn`：自动动作标志位，用于描述系统已经执行的联动动作。
    - `flt`：故障码数组，用于描述传感器或模块故障状态。

    ## B.3 下行控制消息格式

    当前系统下行消息主要包括设备控制、阈值设置与阈值恢复三类。

    1. 设备控制消息：

    ```json
    {"code": 4, "name": "风扇开启"}
    ```

    2. 阈值恢复消息：

    ```json
    {"cmd": "reset_th"}
    ```

    3. 阈值设置消息：

    ```json
    {
      "th": {
        "ta": 38,
        "tb": 10,
        "ha": 85,
        "hb": 20,
        "pa": 150,
        "aa": 200,
        "ca": 1200,
        "fa": 10
      }
    }
    ```

    ## B.4 主题说明

    - `test001up`：主业务主题，用于承载状态上报和控制下发。
    - `WebQT1`：辅助主题，用于承载 Web 一键求助消息。

    ## B.5 协议设计特点

    本系统协议设计体现出两个特点：其一，上行数据采用统一状态快照结构，便于 Qt 与 Web 多端共享同一数据语义；其二，下行命令保留了结构化命令与简化控制命令两类表达方式，既兼顾当前图形界面的实现需求，也保留了底层兼容能力。
    """
).strip()


REFERENCES = [
    "[1] 应急管理部. 2024年全国自然灾害基本情况[R]. 北京: 应急管理部, 2025.",
    "[2] 物联网环境监测系统的设计与实现[J]. 信息记录材料, 2025. DOI:10.16009/j.cnki.cn13-1295/tq.2025.07.017.",
    "[3] 基于物联网的智能温棚环境监测系统研制[D]. 中国优秀硕士学位论文全文数据库, 2022.",
    "[4] Atzori L, Iera A, Morabito G. The Internet of Things: A survey[J]. Computer Networks, 2010, 54(15): 2787-2805.",
    "[5] OASIS. MQTT Version 3.1.1[S/OL]. 2014. https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html.",
    "[6] Yassein M B, Shatnawi M Q, et al. Application layer protocols for the Internet of Things: A survey[C]//ICEMIS 2016. IEEE, 2016: 1-6.",
    "[7] Fahim M, El Mhouti A, et al. Modeling and implementation of a low-cost IoT-smart weather monitoring station and air quality assessment based on fuzzy inference model and MQTT protocol[J]. Modeling Earth Systems and Environment, 2023, 9: 4085-4102.",
    "[8] 基于STM32的环境监测报警系统设计[J]. 软件工程, 2024(06).",
    "[9] 基于MQTT的物联网平台设计与分析[J]. 汉江师范学院学报, 2014(06).",
    "[10] 基于互联网+的智能家居安防管理系统[J]. 中国期刊全文数据库, 2017.",
    "[11] 基于Qt的中央空调远程监控系统的软件设计[J]. 桂林电子科技大学学报, 2017, 37(6):447-451.",
    "[12] 基于ESP32平台和MQTT协议的远程控制系统设计[J]. 软件工程, 2020(08).",
    "[13] 基于云平台的实验室环境监测系统设计[J]. 工业控制计算机, 2023, 36(9):112-114.",
    "[14] 基于MQTT通信协议的无线室内环境实时监测系统设计[J]. 电子测试, 2022, 36(19):23-26.",
    "[15] STMicroelectronics. STM32F103x8/STM32F103xB Datasheet (Doc ID 13587 Rev 17)[Z]. 2021.",
    "[16] Espressif Systems. ESP32 Technical Reference Manual (Version 4.9)[Z]. 2022.",
    "[17] 基于新能源和物联网的校园生态环境监测系统设计[D]. 中国优秀硕士学位论文全文数据库, 2022.",
    "[18] 物联网平台日志分析系统的设计与实现[D]. 中国优秀硕士学位论文全文数据库, 2022.",
    "[19] 基于物联网的机房环境因子监测系统设计与实现[J]. 微型电脑应用, 2023, 39(8):42-45.",
    "[20] 顾沈兵, 何欣, 石峰岭, 等. 社区自然灾害综合监测预警网络应用示范性研究[J]. 职业卫生与应急救援, 2024, 42(4): 522-527.",
    "[21] 基于JavaScript的温湿度监测系统设计与开发[C]. 中国重要会议论文全文数据库, 2023.",
]


def normalize(text: str) -> str:
    return (
        text.replace(" ", "")
        .replace("\u3000", "")
        .replace("\t", "")
        .replace("\n", "")
        .strip()
    )


def delete_paragraph(paragraph):
    element = paragraph._element
    parent = element.getparent()
    if parent is not None:
        parent.remove(element)


def parse_blocks(md: str):
    blocks: list[tuple[str, str]] = []
    current_para: list[str] = []

    def flush_para():
        nonlocal current_para
        if current_para:
            text = "".join([line.strip() for line in current_para]).strip()
            if text:
                blocks.append(("Normal", text))
            current_para = []

    for raw_line in md.splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()
        if stripped == "[[PAGEBREAK]]":
            flush_para()
            blocks.append(("PAGEBREAK", ""))
            continue
        if not stripped:
            flush_para()
            continue
        if stripped.startswith("```"):
            flush_para()
            blocks.append(("CODEFENCE", stripped))
            continue
        if stripped.startswith("### "):
            flush_para()
            blocks.append(("Heading 3", stripped[4:].strip()))
            continue
        if stripped.startswith("## "):
            flush_para()
            blocks.append(("Heading 2", stripped[3:].strip()))
            continue
        if stripped.startswith("# "):
            flush_para()
            blocks.append(("Heading 1", stripped[2:].strip()))
            continue
        if stripped.startswith("- "):
            flush_para()
            blocks.append(("List Bullet", stripped[2:].strip()))
            continue
        if stripped[:2].isdigit() and stripped[1] == "." and len(stripped) > 3:
            flush_para()
            blocks.append(("List Number", stripped))
            continue
        current_para.append(stripped)
    flush_para()
    return blocks


def find_docx(root: Path) -> Path:
    candidates = sorted(root.glob("*.docx"))
    if not candidates:
        raise FileNotFoundError("No docx file found in workspace.")
    for p in candidates:
        if "备份" in p.name and "替换前" not in p.name:
            return p
    return candidates[0]


def insert_blocks_before(anchor_para, blocks):
    for style, text in blocks:
        if style == "PAGEBREAK":
            p = anchor_para.insert_paragraph_before("", style="Normal")
            p.add_run().add_break(WD_BREAK.PAGE)
        elif style == "CODEFENCE":
            anchor_para.insert_paragraph_before(text, style="Normal")
        else:
            anchor_para.insert_paragraph_before(text, style=style)


def locate_paragraph_index(doc: Document, target: str) -> int:
    for i, p in enumerate(doc.paragraphs):
        if normalize(p.text) == normalize(target):
            return i
    raise RuntimeError(f"Paragraph not found: {target}")


def main():
    root = Path("D:/AAA")
    docx_path = find_docx(root)
    backup_path = docx_path.with_name(docx_path.stem + "_章节替换前备份.docx")
    if not backup_path.exists():
        shutil.copy2(docx_path, backup_path)
    else:
        shutil.copy2(backup_path, docx_path)

    doc = Document(str(docx_path))

    # Replace Chinese abstract block.
    abs_idx = locate_paragraph_index(doc, "摘  要")
    eng_abs_idx = locate_paragraph_index(doc, "ABSTRACT")
    abs_anchor = doc.paragraphs[eng_abs_idx]
    for p in list(doc.paragraphs[abs_idx:eng_abs_idx]):
        delete_paragraph(p)
    insert_blocks_before(abs_anchor, parse_blocks(ABSTRACT_CN))

    # Replace English abstract block.
    doc = Document(str(docx_path)) if False else doc
    eng_abs_idx = locate_paragraph_index(doc, "ABSTRACT")
    ch1_idx = next(
        i
        for i, p in enumerate(doc.paragraphs)
        if normalize(p.text).startswith("第1章绪论") or normalize(p.text).startswith("第 1 章 绪论".replace(" ", ""))
    )
    ch1_anchor = doc.paragraphs[ch1_idx]
    for p in list(doc.paragraphs[eng_abs_idx:ch1_idx]):
        delete_paragraph(p)
    insert_blocks_before(ch1_anchor, parse_blocks(ABSTRACT_EN))

    # Replace chapter 1-5 body.
    start_idx = next(
        i
        for i, p in enumerate(doc.paragraphs)
        if normalize(p.text).startswith("第1章绪论") or normalize(p.text).startswith("第 1 章 绪论".replace(" ", ""))
    )
    concl_idx = locate_paragraph_index(doc, "结论")
    conclusion_anchor = doc.paragraphs[concl_idx]
    for p in list(doc.paragraphs[start_idx:concl_idx]):
        delete_paragraph(p)
    insert_blocks_before(conclusion_anchor, parse_blocks(BODY_MD))

    # Replace conclusion.
    concl_idx = locate_paragraph_index(doc, "结论")
    ref_idx = locate_paragraph_index(doc, "参考文献")
    ref_anchor = doc.paragraphs[ref_idx]
    for p in list(doc.paragraphs[concl_idx:ref_idx]):
        delete_paragraph(p)
    insert_blocks_before(ref_anchor, parse_blocks(CONCLUSION_MD))

    # Replace reference list.
    ref_idx = locate_paragraph_index(doc, "参考文献")
    thanks_idx = locate_paragraph_index(doc, "致谢")
    thanks_anchor = doc.paragraphs[thanks_idx]
    for p in list(doc.paragraphs[ref_idx + 1 : thanks_idx]):
        delete_paragraph(p)
    ref_blocks = [("Normal", ref) for ref in REFERENCES]
    insert_blocks_before(thanks_anchor, ref_blocks)

    # Replace thanks and appendices.
    thanks_idx = locate_paragraph_index(doc, "致谢")
    appendix_a_idx = next(
        i for i, p in enumerate(doc.paragraphs) if normalize(p.text).startswith("附录A")
    )
    appendix_a_anchor = doc.paragraphs[appendix_a_idx]
    for p in list(doc.paragraphs[thanks_idx:appendix_a_idx]):
        delete_paragraph(p)
    insert_blocks_before(appendix_a_anchor, parse_blocks(THANKS_MD))

    # Replace appendix A to end with new appendices.
    appendix_a_idx = next(
        i for i, p in enumerate(doc.paragraphs) if normalize(p.text).startswith("附录A")
    )
    for p in list(doc.paragraphs[appendix_a_idx:]):
        delete_paragraph(p)
    if not doc.paragraphs:
        raise RuntimeError("Document unexpectedly empty after cleanup.")
    tail_anchor = doc.add_paragraph("", style="Normal")
    append_blocks = parse_blocks(APPENDIX_A_MD + "\n[[PAGEBREAK]]\n" + APPENDIX_B_MD)
    insert_blocks_before(tail_anchor, append_blocks)
    delete_paragraph(tail_anchor)

    doc.save(str(docx_path))
    print(f"UPDATED: {docx_path}")
    print(f"BACKUP: {backup_path}")


if __name__ == "__main__":
    main()
