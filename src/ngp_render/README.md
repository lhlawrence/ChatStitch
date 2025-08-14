## ngp_render

### Nerf 空间坐标系与车辆载体坐标系之间的变换

- 旋转变换：`body_nerf_rotation_x`、`body_nerf_rotation_y`、`body_nerf_rotation_z`
  - 含义：车辆载体坐标系到 Nerf 空间坐标系的旋转变换
  - 计算方式：Nerf 空间坐标系经过怎样的旋转运动与车辆载体坐标系重合，车辆载体坐标系到 Nerf 空间坐标系的旋转变换就是怎样的
  - 旋转顺序：自行定义，需要修改程序
- 平移变换：`nerf_body_translation_x`、`nerf_body_translation_y`、`nerf_body_translation_z`
  - 含义：Nerf 空间坐标系到车辆载体坐标系的平移变换
  - 计算方式：Nerf 空间坐标系原点在车辆载体坐标系中的坐标