# smart/models.py
from django.db import models
from typing import Optional
from django.utils import timezone


##### 开启APP广告页表模型  ####
class Welcome(models.Model):
    id: Optional[int]  # 添加类型注解
    img = models.ImageField(upload_to='welcome', default='/welcome/slash.jpg')
    order = models.IntegerField()
    create_time = models.DateTimeField(auto_now_add=True)
    is_delete = models.BooleanField(default=False)

    class Meta:
        verbose_name_plural = '欢迎页图片'

    def __str__(self):
        return str(self.img)


# 轮播图
class Banners(models.Model):
    img = models.ImageField(upload_to='banners', default='/banners/banner1.jpg', verbose_name='图片')
    order = models.IntegerField(verbose_name='顺序')
    create_time = models.DateTimeField(auto_now_add=True, verbose_name='创建时间')
    is_delete = models.BooleanField(default=False, verbose_name='是否删除')

    class Meta:
        verbose_name_plural = '轮播图'

    def __str__(self):
        return str(self.img)


# 通知表
class Notice(models.Model):
    title = models.CharField(max_length=64, verbose_name='公告标题')
    content = models.TextField(verbose_name='内容')
    img = models.ImageField(upload_to='notice', default='notice.jpg', verbose_name='公告图片')
    create_time = models.DateTimeField(auto_now=True, verbose_name='创建时间')

    class Meta:
        verbose_name_plural = '公告表'

    def __str__(self):
        return str(self.title)


# 食物推荐表
class Food(models.Model):
    name = models.CharField(max_length=64, verbose_name='食物名称')
    price = models.DecimalField(max_digits=8, decimal_places=2, verbose_name='价格')

    # 营养信息（每100克）
    calorie_per_100g = models.IntegerField(default=0, verbose_name='热量（千卡/100g）')
    protein_per_100g = models.IntegerField(default=0, verbose_name='蛋白质（克/100g）')
    fat_per_100g = models.IntegerField(default=0, verbose_name='脂肪（克/100g）')
    carb_per_100g = models.IntegerField(default=0, verbose_name='碳水化合物（克/100g）')

    # 调料额外热量
    sauce_calorie = models.IntegerField(default=0, verbose_name='酱料热量（千卡）')
    oil_calorie = models.IntegerField(default=0, verbose_name='油热量（千卡）')

    # 默认份量
    default_weight = models.IntegerField(default=200, verbose_name='默认份量（克）')

    spiciness = models.IntegerField(default=0, verbose_name='辣度（0-5）')
    rating = models.DecimalField(max_digits=3, decimal_places=2, default=4.0, verbose_name='推荐指数')
    img = models.ImageField(upload_to='foods', default='/foods/default.jpg', verbose_name='食物图片')
    description = models.TextField(verbose_name='描述')
    order = models.IntegerField(default=0, verbose_name='排序')
    is_delete = models.BooleanField(default=False, verbose_name='是否删除')
    create_time = models.DateTimeField(auto_now_add=True, verbose_name='创建时间')

    class Meta:
        verbose_name_plural = '食物推荐'

    def __str__(self):
        return self.name


# 用户余额模型
class UserBalance(models.Model):
    user_id = models.CharField(max_length=100, unique=True, verbose_name='用户ID')
    balance = models.DecimalField(max_digits=10, decimal_places=2, default=0.00, verbose_name='余额')
    update_time = models.DateTimeField(auto_now=True, verbose_name='更新时间')

    class Meta:
        verbose_name_plural = '用户余额'

    def __str__(self):
        return f"{self.user_id}: {self.balance}"


# 账单记录模型
class Bill(models.Model):
    TYPE_CHOICES = [
        ('income', '收入'),
        ('expense', '支出'),
    ]

    user_id = models.CharField(max_length=100, verbose_name='用户ID')
    type = models.CharField(max_length=10, choices=TYPE_CHOICES, verbose_name='类型')
    type_text = models.CharField(max_length=50, verbose_name='类型描述')
    amount = models.DecimalField(max_digits=10, decimal_places=2, verbose_name='金额')
    balance_after = models.DecimalField(max_digits=10, decimal_places=2, verbose_name='交易后余额')
    description = models.CharField(max_length=200, blank=True, verbose_name='描述')
    create_time = models.DateTimeField(auto_now_add=True, verbose_name='创建时间')

    class Meta:
        verbose_name_plural = '账单记录'
        ordering = ['-create_time']

    def __str__(self):
        return f"{self.user_id} - {self.type} - {self.amount}"


# 用户用餐记录
class UserMealRecord(models.Model):
    user_id = models.CharField(max_length=100, verbose_name='用户ID')
    food_id = models.IntegerField(verbose_name='菜品ID')
    food_name = models.CharField(max_length=100, verbose_name='菜品名称')
    food_img = models.CharField(max_length=200, blank=True, verbose_name='菜品图片')
    price = models.DecimalField(max_digits=8, decimal_places=2, verbose_name='价格')
    weight = models.IntegerField(default=200, verbose_name='份量(克)')
    calorie = models.IntegerField(verbose_name='实际摄入热量')
    protein = models.IntegerField(default=0, verbose_name='蛋白质(克)')
    meal_type = models.CharField(max_length=20, default='lunch',
                                 choices=[('breakfast', '早餐'), ('lunch', '午餐'), ('dinner', '晚餐'),
                                          ('snack', '加餐')],
                                 verbose_name='餐别')
    eat_date = models.DateField(auto_now_add=True, verbose_name='用餐日期')
    eat_time = models.DateTimeField(auto_now_add=True, verbose_name='用餐时间')

    class Meta:
        verbose_name_plural = '用户用餐记录'
        ordering = ['-eat_time']

    def __str__(self):
        return f"{self.user_id} - {self.food_name} - {self.eat_date}"


# 卡余额表模型（注意：模型类需要定义在 models.py 中）
class RfidCard(models.Model):
    uid = models.CharField(max_length=20, unique=True)
    balance = models.DecimalField(max_digits=10, decimal_places=2, default=0)
    created_at = models.DateTimeField(auto_now_add=True)
    is_active = models.BooleanField(default=True, verbose_name='是否激活')

    def __str__(self):
        return f"RfidCard({self.uid}, balance={self.balance})"


class User(models.Model):
    openid = models.CharField(max_length=100, unique=True, verbose_name='微信openid')
    nickname = models.CharField(max_length=100, blank=True, verbose_name='昵称')
    avatar = models.CharField(max_length=500, blank=True, verbose_name='头像')
    phone = models.CharField(max_length=20, blank=True, verbose_name='手机号')

    rfid_card = models.OneToOneField(
        RfidCard,
        on_delete=models.SET_NULL,
        null=True,
        blank=True,
        related_name='user',
        verbose_name='绑定的RFID卡'
    )

    token = models.CharField(max_length=100, blank=True, verbose_name='登录token')
    token_expire = models.DateTimeField(null=True, blank=True, verbose_name='token过期时间')
    is_profile_completed = models.BooleanField(default=False, verbose_name='是否完善资料')

    # 健康信息字段
    height = models.IntegerField(null=True, blank=True, verbose_name='身高(cm)')  # cm
    weight = models.IntegerField(null=True, blank=True, verbose_name='体重(kg)')  # kg
    body_fat = models.IntegerField(null=True, blank=True, verbose_name='体脂率(%)')  # %
    age = models.IntegerField(null=True, blank=True, verbose_name='年龄')
    gender = models.CharField(max_length=10, blank=True, verbose_name='性别')  # male/female
    activity_level = models.CharField(max_length=20, blank=True, verbose_name='活动水平')
    diet_goal = models.CharField(max_length=20, blank=True, verbose_name='饮食目标')
    allergies = models.TextField(blank=True, verbose_name='过敏信息')
    diseases = models.TextField(blank=True, verbose_name='疾病信息')

    create_time = models.DateTimeField(auto_now_add=True, verbose_name='注册时间')
    last_login = models.DateTimeField(auto_now=True, verbose_name='最后登录')


    class Meta:
        verbose_name_plural = '用户表'

    def __str__(self):
        return self.nickname or self.openid


class CardBinding(models.Model):
    """卡绑定记录表"""
    STATUS_CHOICES = [
        ('pending', '待绑定'),
        ('active', '已激活'),
        ('inactive', '已停用'),
        ('unbound', '已解绑'),
    ]

    user = models.ForeignKey(
        User,
        on_delete=models.CASCADE,
        related_name='card_bindings',
        verbose_name='用户'
    )
    card = models.ForeignKey(
        RfidCard,
        on_delete=models.CASCADE,
        related_name='bindings',
        verbose_name='RFID卡'
    )
    status = models.CharField(
        max_length=20,
        choices=STATUS_CHOICES,
        default='pending',
        verbose_name='绑定状态'
    )
    # 绑定时的余额快照
    initial_balance = models.DecimalField(
        max_digits=10,
        decimal_places=2,
        default=0,
        verbose_name='绑定时的余额'
    )
    bind_time = models.DateTimeField(auto_now_add=True, verbose_name='绑定时间')
    unbind_time = models.DateTimeField(null=True, blank=True, verbose_name='解绑时间')
    remark = models.CharField(max_length=200, blank=True, verbose_name='备注')

    class Meta:
        verbose_name_plural = '卡绑定记录'
        unique_together = ['user', 'card']  # 同一用户不能重复绑定同一张卡
        ordering = ['-bind_time']

    def __str__(self):
        return f"{self.user.nickname} - {self.card.uid} ({self.get_status_display()})"


class BindWaitQueue(models.Model):
    """绑定等待队列（用于扫码绑定）"""
    openid = models.CharField(max_length=64, unique=True, verbose_name='微信用户')
    card_uid = models.CharField(max_length=20, blank=True, verbose_name='卡号（绑定后回填）')
    qrcode_token = models.CharField(max_length=100, unique=True, verbose_name='二维码token')
    expire_at = models.DateTimeField(verbose_name='过期时间')
    created_at = models.DateTimeField(auto_now_add=True, verbose_name='创建时间')
    is_used = models.BooleanField(default=False, verbose_name='是否已使用')

    class Meta:
        verbose_name_plural = '绑定等待队列'
        ordering = ['-created_at']

    def __str__(self):
        return f"{self.openid} - {self.qrcode_token}"

    def is_expired(self):
        """检查是否过期"""
        return timezone.now() > self.expire_at

    def generate_token(self):
        """生成二维码token"""
        import hashlib
        import time
        raw = f"{self.openid}_{time.time()}_{hashlib.md5(self.openid.encode()).hexdigest()}"
        return hashlib.sha256(raw.encode()).hexdigest()[:32]


class CardTransaction(models.Model):
    """卡交易记录表"""
    TRANSACTION_TYPES = [
        ('bind', '绑定'),
        ('unbind', '解绑'),
        ('recharge', '充值'),
        ('deduct', '消费'),
        ('refund', '退款'),
    ]

    card = models.ForeignKey(
        RfidCard,
        on_delete=models.CASCADE,
        related_name='transactions',
        verbose_name='RFID卡'
    )
    user = models.ForeignKey(
        User,
        on_delete=models.SET_NULL,
        null=True,
        related_name='card_transactions',
        verbose_name='操作用户'
    )
    transaction_type = models.CharField(
        max_length=20,
        choices=TRANSACTION_TYPES,
        verbose_name='交易类型'
    )
    amount = models.DecimalField(
        max_digits=10,
        decimal_places=2,
        verbose_name='交易金额'
    )
    balance_before = models.DecimalField(
        max_digits=10,
        decimal_places=2,
        verbose_name='交易前余额'
    )
    balance_after = models.DecimalField(
        max_digits=10,
        decimal_places=2,
        verbose_name='交易后余额'
    )
    description = models.CharField(max_length=200, blank=True, verbose_name='描述')
    create_time = models.DateTimeField(auto_now_add=True, verbose_name='交易时间')

    class Meta:
        verbose_name_plural = '卡交易记录'
        ordering = ['-create_time']

    def __str__(self):
        return f"{self.card.uid} - {self.get_transaction_type_display()} - {self.amount}元"

class CardRecord(models.Model):
    """刷卡记录表：存每次刷卡的用户信息"""
    card_uid = models.CharField(max_length=20,
                                    verbose_name='卡号')
    user_id = models.IntegerField(verbose_name='用户ID')
    user_name = models.CharField(max_length=20,
                                     verbose_name='用户名')
    balance = models.DecimalField(max_digits=10, decimal_places=2, default=0,
                                      verbose_name='余额')
    total_price = models.DecimalField(max_digits=10, decimal_places=2, default=0,
                                          verbose_name='消费金额')
    create_time = models.DateTimeField(auto_now_add=True,
                                           verbose_name='刷卡时间')

    class Meta:
        verbose_name_plural = '刷卡记录'
        ordering = ['-create_time']

    def __str__(self):
        return f"{self.card_uid} - {self.user_name} - {self.create_time}"


#  聊天记录模型
class ChatHistory(models.Model):
    openid = models.CharField(max_length=100,verbose_name='用户openid')
    messages = models.TextField(default='[]',verbose_name='聊天记录JSON')
    update_time = models.DateTimeField(auto_now=True,verbose_name='更新时间')

    class Meta:
        verbose_name_plural = '聊天记录'


class PendingCommand(models.Model):
    card_uid = models.CharField(max_length=20)
    cmd_type = models.CharField(max_length=20, default='recharge')
    amount = models.DecimalField(max_digits=10, decimal_places=2)
    status = models.CharField(max_length=10, default='pending')
    create_time = models.DateTimeField(auto_now_add=True)

    class Meta:
        verbose_name_plural = '待执行指令'


