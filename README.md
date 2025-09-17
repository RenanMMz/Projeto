### PROJETO - Nome original, não roubar

"Eu deveria fazer um Projeto" - John Touhou

Iniciei este projeto para aprender sobre renderização em directx. A ideia começou como um Arkanoid / Breakout, mas decidi deixar similar a Touhou Highly Responsive To Prayers

### Controles:

- Setas esquerda e direita para movimentar a "barra"
- Z para atirar, os tiros servem para rebater a bolinha
- X para criar um escudo circular na frente da barrinha
- Direção + X para dar uma "rasteira" no chão que serve para levantar a bola

### To-do:

- Adicionar objetos "quebráveis" que devem ser atingidos pela bolinha (e não pelos tiros)
- Ajustar física conforme necessário para balanceamento, atualmente eu acredito que a bolinha pode estar rápida demais mas preciso testar conforme as demais funcionalidades forem implementadas.
- Fazer com que a colisão da "barra" com a bola tenha efeito negativo (redução de vida), a ideia é que a bolinha só será rebatida por tiros e por comandos de movimentação que serão adicionados futuramente (exemplo: uma rasteira no chão para levantar a bolinha do chão).
- Adicionar HUD
- Adicionar um Menu com Iniciar / Opções / Fechar / etc
- Implementar o conceito de Stages / Bosses
- Adicionar gráficos e sons
- Se possível, implementar um sistema de Replays que replica os comandos utilizados. Minha teoria é de para isso será necessário criar uma engine onde todos os eventos são baseados em frames e salvar todos os comandos utilizados em cada frame. Possível que exista uma forma mais simples, mas o importante é que o Replay não deve salvar um vídeo e sim executar os eventos novamente dentro da própria engine.