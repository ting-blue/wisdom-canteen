from libs.PipeLine import PipeLine
from libs.YOLO import YOLOv8
from libs.Utils import *

from machine import UART, FPIOA
import gc
import json
import utime

if __name__ == "__main__":

    # =====================================
    # kmodel路径
    # =====================================
    kmodel_path = "/data/kmodels/best(3).kmodel"

    # =====================================
    # 类别名称
    # =====================================
    labels = [
        "tomato_scrambled_egg",
        "spicy_sour_potato_shreds",
        "steamed_bun",
        "shaomai",
        "pure_milk",
        "egg"
    ]

    # =====================================
    # 模型输入尺寸
    # =====================================
    model_input_size = [320, 320]

    # =====================================
    # 摄像头分辨率
    # =====================================
    rgb888p_size = [640, 360]

    # =====================================
    # 阈值（建议提高，减少误检）
    # =====================================
    confidence_threshold = 0.2
    nms_threshold = 0.6

    # =====================================
    # 初始化显示
    # =====================================
    pl = PipeLine(
        rgb888p_size=rgb888p_size,
        display_mode="lcd"
    )

    pl.create()

    display_size = pl.get_display_size()

    # =====================================
    # 初始化YOLOv8
    # =====================================
    yolo = YOLOv8(
        task_type="detect",
        mode="video",

        kmodel_path=kmodel_path,

        labels=labels,

        rgb888p_size=rgb888p_size,

        model_input_size=model_input_size,

        display_size=display_size,

        conf_thresh=confidence_threshold,

        nms_thresh=nms_threshold,

        max_boxes_num=10,

        debug_mode=0
    )

    # 配置预处理
    yolo.config_preprocess()

    # =====================================
    # 初始化UART1
    # =====================================
    fpioa = FPIOA()

    # UART1_TX -> GPIO3
    fpioa.set_function(3, FPIOA.UART1_TXD)

    # UART1_RX -> GPIO4
    fpioa.set_function(4, FPIOA.UART1_RXD)

    uart = UART(UART.UART1, 115200)

    print("===================================")
    print("K230 四宫格智能餐盘识别启动")
    print("UART1 初始化成功")
    print("===================================")

    # 发送启动信息
    uart.write("K230_READY\r\n")

    # =====================================
    # 四宫格分界线
    # =====================================
    split_x = 320
    split_y = 140

    # =====================================
    # 串口发送控制
    # =====================================
    last_send_time = 0

    # 200ms发送一次
    send_interval = 200

    # =====================================
    # GC控制（优化性能）
    # =====================================
    frame_count = 0

    try:

        while True:

            # =====================================
            # 降低GC频率（优化）
            # =====================================
            frame_count += 1

            if frame_count % 30 == 0:
                gc.collect()

            # =====================================
            # 获取摄像头图像
            # =====================================
            img = pl.get_frame()

            if img is None:
                continue

            # =====================================
            # 【关键修复】
            # 每帧先清空OSD
            # 防止检测框残留
            # =====================================
            pl.osd_img.clear()

            # =====================================
            # YOLO推理
            # =====================================
            try:

                res = yolo.run(img)

            except Exception as e:

                print("YOLO推理失败:", e)

                continue

            # 当前帧检测结果
            detection_results = []

            # =====================================
            # 解析检测结果
            # =====================================
            if res and len(res) == 3:

                boxes = res[0]
                class_ids = res[1]
                scores = res[2]

                # 防止为空
                if (boxes is not None and
                    class_ids is not None and
                    scores is not None):

                    # 有目标
                    if len(boxes) > 0:

                        for i in range(len(boxes)):

                            try:

                                # =========================
                                # 防止数组越界
                                # =========================
                                if i >= len(class_ids):
                                    continue

                                if i >= len(scores):
                                    continue

                                box = boxes[i]

                                # =========================
                                # 判空
                                # =========================
                                if box is None:
                                    continue

                                if class_ids[i] is None:
                                    continue

                                if scores[i] is None:
                                    continue

                                # =========================
                                # 长度检查
                                # =========================
                                if len(box) < 4:
                                    continue

                                # =========================
                                # YOLO输出
                                # [cx, cy, w, h]
                                # =========================
                                cls_id = int(class_ids[i])

                                score = float(scores[i])

                                cx = int(box[0])
                                cy = int(box[1])

                                w = int(box[2])
                                h = int(box[3])

                                # =========================
                                # 数据合法性检查
                                # =========================
                                if cls_id < 0:
                                    continue

                                if cls_id >= len(labels):
                                    continue

                                if w <= 0 or h <= 0:
                                    continue

                                # =========================
                                # 类别名称
                                # =========================
                                label = labels[cls_id]

                                # =========================
                                # 四宫格位置判断
                                # =========================
                                if cx < split_x and cy < split_y:

                                    position = "top_left"

                                elif cx < split_x and cy >= split_y:

                                    position = "bottom_left"

                                elif cx >= split_x and cy < split_y:

                                    position = "top_right"

                                else:

                                    position = "bottom_right"

                                # =========================
                                # 保存检测结果
                                # =========================
                                detection_results.append({

                                    "label": label,

                                    "confidence": round(score, 2),

                                    "position": position,

                                    "cx": cx,

                                    "cy": cy
                                })

                                # =========================
                                # 串口打印
                                # =========================
                                print("======================")
                                print("食物类别:", label)
                                print("置信度:", round(score, 2))
                                print("中心点:", cx, cy)
                                print("餐盘位置:", position)

                            except Exception as e:

                                print("单目标解析失败:", e)

                                continue

                        # =====================================
                        # 【关键】
                        # 有目标才画框
                        # =====================================
                        if len(detection_results) > 0:

                            try:

                                yolo.draw_result(
                                    res,
                                    pl.osd_img
                                )

                            except Exception as e:

                                print("绘图失败:", e)

            # =====================================
            # UART发送（200ms限流）
            # =====================================
            current_time = utime.ticks_ms()

            if current_time - last_send_time >= send_interval:

                last_send_time = current_time

                try:

                    send_data = {

                        "ts": current_time,

                        "cnt": len(detection_results),

                        "items": detection_results
                    }

                    # K230 json.dumps不支持ensure_ascii
                    json_str = json.dumps(send_data)

                    json_str += "\r\n"

                    uart.write(json_str.encode("utf-8"))

                    print(
                        "[UART SEND]",
                        len(detection_results),
                        "items"
                    )

                except Exception as e:

                    print("串口发送失败:", e)

            # =====================================
            # 显示图像
            # =====================================
            pl.show_image()

    except KeyboardInterrupt:

        print("用户退出")

        try:
            uart.write("K230_STOP\r\n")
        except:
            pass

    except Exception as e:

        print("运行错误:", e)

        try:

            uart.write(
                ("ERROR:" + str(e) + "\r\n").encode("utf-8")
            )

        except:
            pass

    finally:

        print("正在释放资源...")

        try:
            yolo.deinit()
        except:
            pass

        try:
            pl.destroy()
        except:
            pass

        try:
            uart.deinit()
        except:
            pass

        print("程序结束")
