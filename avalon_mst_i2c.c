#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/wait.h>


/* This data structure contains all the necessary data to manage the I2C master */
struct avalon_mst_i2c_dev {
  void __iomem *base;
  int irq;
  struct device *dev;
  struct i2c_adapter adapter;
  wait_queue_head_t wq;
  int xfer_dodo;
};


/* Transfer one i2c message */
static int avalon_mst_i2c_xfer_msg(struct avalon_mst_i2c_dev *adev, struct i2c_msg *msg)
{
  u8 addr = i2c_8bit_addr_from_msg(msg);
  u8 val, buf;
  int i;
  adev->xfer_dodo = 0;

  dev_info(adev->dev, "i2c transfer message, @=%d, len=%d\n", addr, msg->len);

  if (msg->flags & I2C_M_RD) {
    /* TODO read */

    //Nb d'octets à lire
    writeb(msg->len, adev->base + 0x8);

    //A envoyer au client 
    //writeb(addr | 0x1, adev->base);
    
    writeb(addr , adev->base);
    
    //Je me mets en attente de l'interruption
    /*** Methode 1 : attente active ***
    do{
      val = readb(adev->base + 0xC) & 0x3F;
      dev_info(adev->dev, "en attente de l'interrupt = 0x%X\n",val);
    } while((val & 0x02) != 2);
    ***/

    /*** Methode 2 : attente passive ***/
    adev->xfer_dodo = 0;
    //wait_event_interruptible(adev->wq, ((readb(adev->base + 0xC) & 0x3F & 0x02) == 2));
    wait_event_interruptible(adev->wq, (adev->xfer_dodo == 1));
    
    //L'interruption est reçue
    writeb(val & 0x3D, adev->base + 0xC);

    //On lit les données contenues dans la fifo
    for(i=0 ; i<msg->len ; i++){
      msg->buf[i] = readb(adev->base + 0x4);
      dev_info(adev->dev, "donnée reçue en lecture %d : 0x%X \n", i, msg->buf[i]);
    }

    //Au final, on aura reçu 
    u8 nb_data = readb(adev->base + 0x8);
    dev_info(adev->dev, "NB donnée totale reçue : 0x%x \n", nb_data);
  
  }
  else {
    /* TODO write */

    //On écrit d'abord dans la fifo
    for(i=0 ; i<msg->len ; i++){
      writeb(msg->buf[i], adev->base + 0x4);
      dev_info(adev->dev, "donnée envoyée en écriture %d : %X \n", i, msg->buf[i]);
    }

    //On envoit l'adresse du client
    writeb(addr, adev->base);

    //Je me mets en attente de l'interruption
    /*** Methode 1 : attente active ***
    do{
      val = readb(adev->base + 0xC) & 0x3F;
      dev_info(adev->dev, "en attente de l'état d'interruption = 0x%x\n",val);
    } while((val & 0x02) != 2);
    ***/

    /*** Methode 2 : attente passive ***/
    //wait_event_interruptible(adev->wq,((readb(adev->base + 0xC) & 0x3F & 0x02) == 2));
    wait_event_interruptible(adev->wq, (adev->xfer_dodo == 1));

    //L'interruption est reçue et j'acquitte
    writeb(val & 0x3D, adev->base + 0xC);      
  }

  return 0;
}

/* Clear the interrupt */
static void avalon_mst_i2c_clear_int(struct avalon_mst_i2c_dev *adev) {
  /* TODO */
  pr_info("Je rentre dans clear_interrupt\n");
  u8 val;
  //Attendre l'interruption
  do{
    val = readb(adev->base + 0xC);
  }while((val & 0x02) != 2);

  //Acquittement
  writeb(val & 0x3D, adev->base + 0xC);

  pr_info("L'IRQ est enlevé, je sors de clear_interrupt\n");
}

/* Interrupt handler */
static irqreturn_t avalon_mst_i2c_irq(int irq, void *_dev)
{
  /* TODO */
  struct avalon_mst_i2c_dev *adev = (struct avalon_mst_i2c_dev *) _dev;
  pr_info("J'appelle mon IRQ Handler\n");

  //Mon handler doit se contenter d'acquitter l'interruption
  avalon_mst_i2c_clear_int(adev);

  adev->xfer_dodo = 1;

  //De réveiller un processus bloqué sur la fonction xfer_msg
  //1 à chaque fois car le bus est pris
  wake_up(&adev->wq);
  
  pr_info("Je sors de mon IRQ Handler\n");

  return IRQ_HANDLED;
}

/* Transfer several i2c messages
 * This function is called by the i2c subsystem
 * Value returned: Number of messages transfered or a negative value in case of error
 */
static int avalon_mst_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
  struct avalon_mst_i2c_dev *adev = i2c_get_adapdata(adap);
  int i, ret;

  dev_info(adev->dev, "i2c transfer, number of messages=%d\n", num);

  for (i = 0; i < num; i++) {
    ret = avalon_mst_i2c_xfer_msg(adev, msgs++);
    if (ret)
      return ret;
  }

  return num;
}

/* Function called by the i2c subsystem to learn the capabilities of the adapter */
static u32 avalon_mst_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm avalon_mst_i2c_algo = {
	.master_xfer = avalon_mst_i2c_xfer,
	.functionality = avalon_mst_i2c_func,
};

/* The probe function. Called by the kernel when a compatible device is detected */
static int avalon_mst_i2c_probe(struct platform_device *pdev)
{

  struct avalon_mst_i2c_dev *adev = NULL;
  struct resource *res;
  int ret;

  dev_info(&pdev->dev, "avalon_mst_i2c_probe\n");
  dev_info(&pdev->dev, "Je certifie que le module qui tourne correspond bien à ce code\n");

  /* Allocates the data structure to represent this device */
  adev = devm_kzalloc(&pdev->dev, sizeof(struct avalon_mst_i2c_dev), GFP_KERNEL);
  if (!adev)
    return -ENOMEM;

  /* Retrieves the memory range from the DTB */
  res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

  dev_info(&pdev->dev, "resource->start = %08x, resource->end = %08x\n", res->start, res->end);

  /* Maps this physical memory range to virtual address space */
  adev->base = devm_ioremap_resource(&pdev->dev, res);
  if (IS_ERR(adev->base))
    return PTR_ERR(adev->base);

  /* Retrieves the IRQ number from the DTB */
  adev->irq = platform_get_irq(pdev, 0);
  if (adev->irq < 0) {
    dev_err(&pdev->dev, "missing interrupt resource\n");
    return adev->irq;
  }
  dev_info(&pdev->dev, "IRQ = %d\n", adev->irq);

  /* Register the bottom half of the IRQ handler (threaded IRQ) */
  /*
  ret = devm_request_threaded_irq(&pdev->dev, adev->irq, NULL,
				  avalon_mst_i2c_irq, IRQF_ONESHOT,
				  pdev->name, adev);
  */
  ret = devm_request_irq(&pdev->dev, adev->irq,
				  avalon_mst_i2c_irq, IRQF_SHARED,
				  pdev->name, adev);

  if (ret) {
    dev_err(&pdev->dev, "failed to register IRQ\n");
    return ret;
    }

  adev->dev = &pdev->dev;

  /****** Controler initialization *******/
  //Je fais le resest de mon controler
  writeb((1<<5), adev->base + 0xC);

  //Je remets mon reset à 0
  writeb((0<<5), adev->base + 0xC);

  //Je lis mon statut que j'imprime
  u8 val;
  val = readb(adev->base + 0xC);

  //j'affiche mon status
  dev_info(adev->dev , "Status = %8x \n", val);

  if (val != 0x21)
		pr_info("Erreur : initialisation du controleur\n");
  else
	  pr_info("Initialisation du controleur (attendu : 21)\n");

  //À l'initialisation, il faut juste attendre après le reset que le 
  //signal busy repasse à 0. Vous pouvez faire de l'attente active ici, c'est très rapide
  do{
    val = readb(adev->base + 0xC);
    dev_info(adev->dev , "Status = %8x \n", val);
  } while((val & 0x01) != 0);
  dev_info(adev->dev, "Signal busy à %8x (attendu : 0)\n", val);

  //Choix de la vitesse du périphérique
  //Mode Fast 01 sur 4:3
  //writeb(1<<3, adev->base + 0xC);mode FAST 
  //Mode standard (00 sur 4:3) à remettre car I2C lent et bonne comm maître-esclave
  writeb(0,adev->base + 0xC);
  val = readb(adev->base + 0xC);
  dev_info(adev->dev , "Status FAST (attendu 8) = %8x \n", val);


  /* Initializes the wait queue */
  init_waitqueue_head(&adev->wq);

  /* Registers the device as an I2C adapter (controler) */
  i2c_set_adapdata(&adev->adapter, adev);
  strlcpy(adev->adapter.name, pdev->name, sizeof(adev->adapter.name));
  adev->adapter.owner = THIS_MODULE;
  adev->adapter.algo = &avalon_mst_i2c_algo;
  adev->adapter.dev.parent = &pdev->dev;
  adev->adapter.dev.of_node = pdev->dev.of_node;

  /* Creates a link between the pdev and adev structures */
  platform_set_drvdata(pdev, adev);

  ret = i2c_add_adapter(&adev->adapter);
  if (ret)
    return ret;

  dev_info(&pdev->dev, "avalon_mst_i2c probe complete\n");

  return 0;
}

/* Remove function. Called when the device is "removed" (in our case
 * when the driver is removed with rmmod) */
static int avalon_mst_i2c_remove(struct platform_device *pdev)
{
  struct avalon_mst_i2c_dev *adev = platform_get_drvdata(pdev);

  i2c_del_adapter(&adev->adapter);

  dev_info(&pdev->dev, "avalon_mst_i2c removed!\n");

  return 0;
}

/* Device Tree association */
static const struct of_device_id of_avalon_mst_i2c_match[] = {
    { .compatible = "se,avalon_mst_i2c" },
    {},
};
MODULE_DEVICE_TABLE(of, of_avalon_mst_i2c_match);

/* Description of our platform driver */
static struct platform_driver avalon_mst_i2c_driver = {
    .probe          = avalon_mst_i2c_probe,
    .remove         = avalon_mst_i2c_remove,
    .driver         = {
        .name           = "avalon_mst_i2c",
        .of_match_table = of_avalon_mst_i2c_match,
    },
};

module_platform_driver(avalon_mst_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Small i2c controler");
MODULE_AUTHOR("Guillaume Duc <guillaume.duc@telecom-paris.fr>");



